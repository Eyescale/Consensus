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
cnOperate( CNStory *story, CNDB *db )
{
	listItem *narratives = story;
	if (( narratives == NULL ) || db_out(db) )
		 return 0;
#ifdef DEBUG
	fprintf( stderr, "cnOperate bgn\n" );
#endif
	/* separate narratives into active (base) and tbd
	*/
	OperateData data;
	memset( &data, 0, sizeof(data) );
	CNNarrative *n;
	for ( listItem *i=narratives; i!=NULL; i=i->next ) {
		n = i->ptr;
		if (( n->proto )) addItem( &data.tbd, n );
		else addItem( &data.active, n );
	}
	if ( data.active == NULL ) {
		freeListItem( &data.tbd );
		return 0;
	}

	/* operate active narratives, possibly finding more as we go
	*/
	BMContext *ctx = newContext( NULL, NULL, db );
	while (( n = popListItem( &data.active ) )) {
		data.n = n;
		if (( n->proto )) {
			bm_traverse( n->proto, ctx, operate_CB, &data );
		}
		else operate( n, ctx, &data );
	}
	freeContext( ctx );

	freeListItem( &data.tbd );
#ifdef DEBUG
	fprintf( stderr, "cnOperate end\n" );
#endif
	return 1;
}
static int
operate_CB( CNInstance *e, BMContext *ctx, void *user_data )
{
	OperateData *data = user_data;
	CNNarrative *n = data->n;
	ctx = newContext( n, e, ctx->db );
	operate( n, ctx, data );
	freeContext( ctx );
	return BM_CONTINUE;
}

//===========================================================================
//	operate
//===========================================================================
#define ctrl(e)	case e:	if ( passed ) { j = NULL; break; }
static BMTraverseCB enable_CB;
static void
operate( CNNarrative *narrative, BMContext *ctx, OperateData *data )
{
#ifdef DEBUG
	fprintf( stderr, "operate bgn\n" );
#endif
	listItem *i = newItem( narrative->root ), *stack = NULL;
	int passed = 1, marked = 0;
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
			marked = ( passed && !strncmp( expression, "?:", 2 ));
			break;
		ctrl(ELSE_ON) case ON:
			passed = on_event( expression, ctx );
			marked = ( passed && !strncmp( expression, "?:", 2 ));
			break;
		ctrl(ELSE_EN) case EN:
			if ( !data->tbd ) break;
			bm_traverse( expression, ctx, enable_CB, data );
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
			i = j; marked = 0;
			continue;
		}
		for ( ; ; ) {
			if (( marked )) {
				bm_pop_mark( ctx, '?' );
				marked = 0;
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
					marked = !strncmp( expression, "?:", 2 );
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
#ifdef DEBUG
	fprintf( stderr, "operate end\n" );
#endif
}
static int cmp_CB( CNInstance *, BMContext *, void * );
static int
enable_CB( CNInstance *e, BMContext *ctx, void *user_data )
/*
	compare entity matching narrative local EN declaration
	with entities matching protos "tbd" - in base context
*/
{
	OperateData *data = user_data;
	data->e = e;
	listItem *last_i = NULL, *next_i;
	ctx = newContext( NULL, NULL, ctx->db );
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
	freeContext( ctx );
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
#ifdef DEBUG
	fprintf( stderr, "cndUpdate bgn\n" );
#endif
	db_update( db );
#ifdef DEBUG
	fprintf( stderr, "cnUpdate end\n" );
#endif
}

//===========================================================================
//	in_condition
//===========================================================================
static int
in_condition( char *expression, BMContext *ctx )
{
	int retval;
#ifdef DEBUG
	fprintf( stderr, "in condition bgn: %s\n", expression );
#endif
	if ( !strcmp( expression, "~." ) )
		retval = db_is_empty( 0, ctx->db );
	else
		retval = bm_feel( expression, ctx, BM_CONDITION );
#ifdef DEBUG
	fprintf( stderr, "in_condition end\n" );
#endif
	return retval;
}

//===========================================================================
//	on_event
//===========================================================================
static int
on_event( char *expression, BMContext *ctx )
{
	int retval;
#ifdef DEBUG
	fprintf( stderr, "on_event bgn: %s\n", expression );
#endif
	if ( !strncmp( expression, "~.", 2 ) )
		retval = db_still( ctx->db );
	else if ( !strncmp( expression, "~(", 2 ) )
		retval = bm_feel( expression+1, ctx, BM_RELEASED );
	else if ( !strcmp( expression, "init" ) )
		retval = db_in( ctx->db );
	else 
		retval = bm_feel( expression, ctx, BM_MANIFESTED );
#ifdef DEBUG
	fprintf( stderr, "on_event end\n" );
#endif
	return retval;
}

//===========================================================================
//	do_action
//===========================================================================
static int
do_action( char *expression, BMContext *ctx )
{
#ifdef DEBUG
	fprintf( stderr, "do_action: do %s\n", expression );
#endif
	if ( !strcmp( expression, "exit" ) )
		db_exit( ctx->db );
	else if ( !strncmp( expression, "~(", 2 ) )
		bm_release( expression+1, ctx );
	else if ( strcmp( expression, "." ) )
		bm_instantiate( expression, ctx );
#ifdef DEBUG
	fprintf( stderr, "do_action end\n" );
#endif
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
#ifdef DEBUG
	fprintf( stderr, "do_input bgn: %s\n", expression );
#endif
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
#ifdef DEBUG
	fprintf( stderr, "do_input end\n" );
#endif
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
#ifdef DEBUG
	fprintf( stderr, "do_output bgn: %s\n", expression );
#endif
	expression++; // skipping the leading '>'

	/* extracts format and args:{ expression(s) }
	*/
	char *format;
	switch ( *expression ) {
	case '"':
		format = expression;
		expression = p_prune( PRUNE_FILTER, format );
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
#ifdef DEBUG
	fprintf( stderr, "do_output end\n" );
#endif
	return retval;
}

