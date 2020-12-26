#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "database.h"
#include "expression.h"
#include "traversal.h"

// #define DEBUG

static int in_condition( char *, BMContext * );
static int on_event( char *, BMContext * );
static int do_action( char *, BMContext * );
static int do_input( char *, BMContext * );
static int do_output( char *, BMContext * );

//===========================================================================
//	cnLoad
//===========================================================================
void
cnLoad( char *path, CNDB *db )
{
	char *expression;
	FILE *stream = strcmp(path,"") ? fopen( path, "r" ) : stdin;
	if ( stream == NULL ) {
		fprintf( stderr, "B%%: %s: no such file or directory\n", path );
		return;
	}
	while (( expression = db_input(stream,1) )) {
		bm_substantiate( expression, db );
		free( expression );
	}
	if ( strcmp(path,"") ) fclose( stream );
}

//===========================================================================
//	cnOperate
//===========================================================================
typedef struct {
	listItem *tbd;
	listItem *active;
	CNNarrative *n;
	CNInstance *e;
	int success;
} OperateData;
static BMTraverseCB operate_CB;
static void operate( CNNarrative *, BMContext *, OperateData * );

int
cnOperate( listItem *narratives, CNDB *db )
{
	if (( narratives == NULL ) || db_out(db) )
		 return 0;

	/* determine active (=base, here) narratives, if any
	*/
	OperateData data = { NULL, NULL, NULL, NULL, 0 };
	for ( listItem *i=narratives; i!=NULL; i=i->next ) {
		CNNarrative *n = i->ptr;
		if (( n->proto )) addItem( &data.tbd, n );
		else addItem( &data.active, n );
	}
	if ( data.active == NULL ) {
		freeListItem( &data.tbd );
		return 0;
	}

	/* operate active narratives, possibly finding more as we go
	*/
	BMContext *ctx = bm_push( NULL, NULL, db );
	CNNarrative *n;
	while (( n = popListItem( &data.active ) )) {
		char *proto = n->proto;
		if (( proto )) {
			data.n = n;
			bm_traverse( proto, ctx, operate_CB, &data );
		}
		else operate( n, ctx, &data );
	}
	bm_pop( ctx );

	freeListItem( &data.tbd );
	return 1;
}
static int
operate_CB( CNInstance *e, BMContext *ctx, void *user_data )
{
	OperateData *data = user_data;
	CNNarrative *n = data->n;
	ctx = bm_push( n, e, ctx->db );
	operate( n, ctx, data );
	bm_pop( ctx );
	return BM_CONTINUE;
}

//===========================================================================
//	operate
//===========================================================================
#define ctrl(e)	case e:	if ( passed ) { j = NULL; break; }
static BMTraverseCB activate_CB;
static void
operate( CNNarrative *narrative, BMContext *ctx, OperateData *data )
{
	listItem *i = newItem( narrative->root ), *stack = NULL;
	int passed = 1, pushed = 0;
	for ( ; ; ) {
		CNOccurrence *occurrence = i->ptr;
		char *expression = occurrence->data->expression;
		listItem *j = occurrence->data->sub;
		switch ( occurrence->type ) {
		ctrl(ELSE) case ROOT:
			passed = 1;
			break;
		ctrl(ELSE_IN) case IN:
			passed = in_condition( expression, ctx );
			pushed = ( passed && !strncmp( expression, "?:", 2 ));
			break;
		ctrl(ELSE_ON) case ON:
			passed = on_event( expression, ctx );
			pushed = ( passed && !strncmp( expression, "?:", 2 ));
			break;
		ctrl(ELSE_EN) case EN:
			if (!(data->tbd)) break;
			bm_traverse( expression, ctx, activate_CB, data );
			break;
		ctrl(ELSE_DO) case DO:
			do_action( expression, ctx );
			break;
		ctrl(ELSE_INPUT) case INPUT:
			do_input( expression, ctx );
			break;
		ctrl(ELSE_OUTPUT) case OUTPUT:
			do_output( expression, ctx );
			break;
		case LOCAL:
			bm_register( ctx, expression );
			break;
		}
		if (( j && passed )) {
			addItem( &stack, i );
			i = j; pushed = 0;
			continue;
		}
		for ( ; ; ) {
			if (( pushed )) {
				bm_pop_mark( ctx );
				pushed = 0;
			}
			if (( i->next )) {
				i = i->next;
				break;
			}
			else if (( stack )) {
				i = popListItem( &stack );
				occurrence = i->ptr;
				passed = 1; // otherwise we would not be here
				switch ( occurrence->type ) {
				case IN: case ELSE_IN:
				case ON: case ELSE_ON:
					expression = occurrence->data->expression;
					pushed = !strncmp( expression, "?:", 2 );
					// no break
				default:
					break;
				}
			}
			else goto RETURN;
		}
	}
RETURN:
	freeItem( i );
}
static int cmp_CB( CNInstance *, BMContext *, void * );
static int
activate_CB( CNInstance *e, BMContext *ctx, void *user_data )
/*
	compare entity matching narrative local EN declaration
	with entities matching protos "tbd" - in base context
*/
{
	OperateData *data = user_data;
	ctx = bm_push( NULL, NULL, ctx->db );
	data->e = e;
	listItem *last_i = NULL, *next_i;
	for ( listItem *i=data->tbd; i!=NULL; i=next_i ) {
		next_i = i->next;
		CNNarrative *n = i->ptr;
		bm_traverse( n->proto, ctx, cmp_CB, data );
		if ( data->success ) {
			clipListItem( &data->tbd, i, last_i, next_i );
			addItem( &data->active, n );
			data->success = 0;
		}
		else last_i = i;
	}
	bm_pop( ctx );
	return (data->tbd) ? BM_CONTINUE : BM_DONE;
}
static int
cmp_CB( CNInstance *e, BMContext *ctx, void *user_data )
{
	OperateData *data = user_data;
	if ( e == data->e ) {
		data->success = 1;
		return BM_DONE;
	}
	return BM_CONTINUE;
}

//===========================================================================
//	cnUpdate
//===========================================================================
void
cnUpdate( CNDB *db )
{
	db_update( db );
}

//===========================================================================
//	in_condition
//===========================================================================
static int
in_condition( char *expression, BMContext *ctx )
{
	// fprintf( stderr, "in condition: %s\n", expression );
	if ( !strcmp( expression, "~." ) )
		return db_is_empty( ctx->db );
	else return bm_feel( expression, ctx, BM_CONDITION );
}

//===========================================================================
//	on_event
//===========================================================================
static int
on_event( char *expression, BMContext *ctx )
{
	// fprintf( stderr, "on_event: %s\n", expression );
	if ( !strcmp( expression, "~." ) )
		return db_still( ctx->db );
	else if ( !strncmp( expression, "~(", 2 ) )
		return bm_feel( expression+1, ctx, BM_RELEASED );
	else return bm_feel( expression, ctx, BM_INSTANTIATED );
}

//===========================================================================
//	do_action
//===========================================================================
static int
do_action( char *expression, BMContext *ctx )
{
	// fprintf( stderr, "do_action: do %s\n", expression );
	if ( !strcmp( expression, "exit" ) )
		db_exit( ctx->db );
	else if ( !strncmp( expression, "~(", 2 ) )
		bm_release( expression+1, ctx );
	else if ( strcmp( expression, "." ) )
		bm_instantiate( expression, ctx );
	return 1;
}

//===========================================================================
//	do_input
//===========================================================================
static int
do_input( char *expression, BMContext *ctx )
/*
	Assuming expression is in the form
		variable : format <
	. reads input from stdin according to format
*/
{
	/* extract args:{ expression(s) } and format
	*/
	listItem *args = NULL;
	char *p=expression, *format;
	do {
		for ( char *q = p_prune( PRUNE_FILTER, p ); *q==':'; p=q, q=q+1 )
			q = p_prune( PRUNE_FILTER, q );
		*p = (char) 0;
		addItem( &args, newPair( expression, p ));
	}
	while ( 0 ); // provisional
	reorderListItem( &args );
	format = p + 1;

	/* invoke bm_inputf()
	*/
	int retval = ( *format != '"' ) ?
		bm_inputf( "", args, ctx ) :
		bm_inputf( format, args, ctx );

	/* cleanup
	*/
	Pair *pair;
	while (( pair = popListItem( &args ) )) {
		p = pair->value;
		*p = ':';
		freePair( pair );
	}
	return retval;
}

//===========================================================================
//	do_output
//===========================================================================
static int
do_output( char *expression, BMContext *ctx )
/*
	Assuming expression is in the form
		> format : expression(s)
	then outputs expression(s) to stdout according to format
*/
{
	expression++; // skipping the leading '>'

	/* extracts format and args:{ expression(s) }
	*/
	char *format;
	switch ( *expression ) {
	case '"':
		format = expression;
		expression = p_prune( PRUNE_FORMAT, format );
		if ( *expression ) expression++; // skipping ':'
		break;
	case ':':
		expression++;
		// no break
	default:
		format = "";
	}
	listItem *args = NULL;
	if ( *expression ) addItem( &args, expression );

	/* invoke bm_outputf()
	*/
	int retval = bm_outputf( format, args, ctx );

	/* cleanup
	*/
	freeListItem( &args );
	return retval;
}

