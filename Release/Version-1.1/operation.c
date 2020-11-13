#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "expression.h"
#include "traversal.h"
#include "util.h"

// #define DEBUG

static int in_condition( char *, BMContext * );
static int on_event( char *, BMContext * );
static int do_action( char *, BMContext * );
static int do_input( char *, BMContext * );
static int do_output( char *, BMContext * );

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
#ifdef DEBUG
fprintf( stderr, "=============================\n" );
#endif
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
	int passed = 1;
	for ( ; ; ) {
		CNOccurrence *occurrence = i->ptr;
		listItem *j = occurrence->data->sub;
		switch ( occurrence->type ) {
		ctrl(ELSE) case ROOT:
			passed = 1;
			break;
		ctrl(ELSE_IN) case IN:
			passed = in_condition( occurrence->data->expression, ctx );
			break;
		ctrl(ELSE_ON) case ON:
			passed = on_event( occurrence->data->expression, ctx );
			break;
		ctrl(ELSE_EN) case EN:
			if (!(data->tbd)) break;
			char *expression = occurrence->data->expression;
			bm_traverse( expression, ctx, activate_CB, data );
			break;
		ctrl(ELSE_DO) case DO:
			do_action( occurrence->data->expression, ctx );
			break;
		ctrl(ELSE_INPUT) case INPUT:
			do_input( occurrence->data->expression, ctx );
			break;
		ctrl(ELSE_OUTPUT) case OUTPUT:
			do_output( occurrence->data->expression, ctx );
			break;
		case LOCAL:
			bm_register( ctx, occurrence->data->expression, NULL );
			break;
		}
		if (( j && passed )) {
			addItem( &stack, i );
			i = j; continue;
		}
		for ( ; ; ) {
			if (( i->next )) {
				i = i->next;
				occurrence = i->ptr;
				break;
			}
			else if (( stack )) {
				i = popListItem( &stack );
				passed = 1; // otherwise we would not be here
			}
			else goto RETURN;
		}
	}
RETURN:
	while (( stack )) {
		i = popListItem( &stack );
	}
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
	return bm_feel( expression, ctx, BM_CONDITION );
}

//===========================================================================
//	on_event
//===========================================================================
static int
on_event( char *expression, BMContext *ctx )
{
	// fprintf( stderr, "on_event: %s\n", expression );
	if ( !strncmp( expression, "~(", 2 ) ) {
		return bm_feel( expression+1, ctx, BM_RELEASED );
	}
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
		bm_substantiate( expression, ctx );
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
	int retval = ( *format == '<' ) ?
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
	/* extracts format and args:{ expression(s) }
	*/
	char *format = expression + 1; // skipping '>'

	listItem *args = NULL;
	expression = p_prune( PRUNE_FORMAT, format );
	if ( *expression ) expression++; // skipping ':'
	if ( *expression ) addItem( &args, expression );

	/* invoke bm_outputf()
	*/
	int retval = ( *format == ':' ) ?
		bm_outputf( "", args, ctx ) :
		bm_outputf( format, args, ctx );

	/* cleanup
	*/
	freeListItem( &args );
	return retval;
}

