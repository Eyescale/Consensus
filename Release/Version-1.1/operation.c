#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "database.h"
#include "expression.h"
#include "operation.h"
#include "narrative.h"
#include "traversal.h"

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
static void operate( CNOccurrence *, BMContext *, OperateData * );

int
cnOperate( listItem *narratives, CNDB *db )
{
#ifdef DEBUG
fprintf( stderr, "=============================\n" );
#endif
	if (( narratives == NULL ) || db_out(db) )
		 return 0;

	/* determine active narratives, if any
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
		else operate( n->root, ctx, &data );
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
	operate( n->root, ctx, data );
	bm_pop( ctx );
	return BM_CONTINUE;
}

//===========================================================================
//	operate
//===========================================================================
#define ctrl(e)	case e:	if ( passed ) { j = NULL; break; }
static BMTraverseCB activate_CB;
static void
operate( CNOccurrence *occurrence, BMContext *ctx, OperateData *data )
{
	listItem *i = newItem( occurrence ), *stack = NULL;
	int passed = 1;
	for ( ; ; ) {
		occurrence = i->ptr;
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
	. instantiates ((*,variable),input)

	Note: this implementation only processes identifiers in the form
	[A-Z-a-z0-9_]+ treating all other characters as input separators
*/
{
	char *format = NULL, *p;

	// generate ((*,variable), from expression
	CNString *s = newString();
	StringAppend( s, '(' );
	StringAppend( s, '(' );
	StringAppend( s, '*' );
	StringAppend( s, ',' );
	for ( p=expression; *p && ((*p!=':')||p_filtered(p+1)); p++ ) {
		StringAppend( s, *p );
	}
	StringAppend( s, ')' );
	StringAppend( s, ',' );

	format = p+1;	// not used in this version

	// complete ((*,variable),input) with input, where
	// input is read (blocking) according to format
	// Here format is assumed to be default
	int event, newline = 1;
	do {
		switch (( event = getchar() )) {
		case '#':
			if ( newline ) do { event = getchar(); }
			while (( event != EOF ) && ( event != '\n' ));
			break;
		case '\n':
			newline = 1;
			break;
		default:
			newline = 0;
		}
	}
	while (( event != EOF ) && is_separator( event ));
	if ( event == EOF ) {
		// release (*,variable)
		StringAppend( s, ')' );
		expression = StringFinish( s, 0 );
		bm_release( &expression[ 1 ], ctx );
	}
	else {
		// read & instantiate ((*,expression),input)
		do {
			StringAppend( s, event );
			event = getchar();
		}
		while ( !is_separator( event ) );
		StringAppend( s, ')' );
		expression = StringFinish( s, 0 );
		bm_substantiate( expression, ctx );
	}
	freeString( s );
	return 0;
}

//===========================================================================
//	do_output
//===========================================================================
static char * skip_format( char *format );

static int
do_output( char *expression, BMContext *ctx )
/*
	Assuming expression is in the form
		> format : expression
	then outputs expression to stdout according to format
*/
{
	// skip the leading '>'
	char *format = expression + 1;
	switch ( *format ) {
	case ':':
		// no format: output expression results
		expression = format + 1;
		if ( *expression ) {
			bm_output( expression, ctx );
		}
		else printf( "\n" );
		break;
	default:
		expression = skip_format( format );
		switch ( *expression ) {
		case ':':
			expression++;
		}
		bm_outputf( format, expression, ctx );
	}
	return 0;
}
static char *
skip_format( char *format )
{
	int escaped = 0;
	for ( char *p=format; *p; p++ ) {
		switch ( *p ) {
		case '\\':
			switch ( escaped ) {
			case 0:
			case 2:
				escaped++;
				break;
			case 1:
			case 3:
				escaped--;
				break;
			}
			break;
		case '\"':
			switch ( escaped ) {
			case 0 :
				escaped = 2;
				break;
			case 2:
				return p+1;
			case 1:
			case 3:
				escaped--;
				break;
			}
			break;
		default:
			switch ( escaped ) {
			case 1:
			case 3:
				escaped--;
				break;
			}
			break;
		}
	}
	return format;
}
