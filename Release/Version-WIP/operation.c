#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "locate.h"
#include "program.h"
#include "expression.h"
#include "traversal.h"

// #define DEBUG

static int in_condition( char *, BMContext *, int *marked );
static int on_event( char *, BMContext *, int *marked );
static int do_action( char *, BMContext * );
static int do_input( char *, BMContext * );
static int do_output( char *, BMContext * );

//===========================================================================
//	bm_operate / bm_update
//===========================================================================
#define ctrl(e)	case e:	if ( passed ) { j = NULL; break; }

int
bm_operate( Pair *entry, BMContext *ctx, listItem **new, CNStory *story )
{
	listItem *narratives = entry->value;
	CNNarrative *narrative = narratives->ptr;
	CNDB *db = BMContextDB( ctx );
	if ( db_out(db) ) return 0;
#ifdef DEBUG
	fprintf( stderr, "operate bgn\n" );
#endif
	listItem *i = newItem( narrative->root ), *stack = NULL;
	int passed = 1, marked = 0;
	for ( ; ; ) {
		CNOccurrence *occurrence = i->ptr;
		char *expression = occurrence->data->expression;
		listItem *j = occurrence->sub;
		switch ( occurrence->data->type ) {
		ctrl(ELSE) case ROOT:
			passed = 1;
			break;
		ctrl(ELSE_IN) case IN:
			passed = in_condition( expression, ctx, &marked );
			break;
		ctrl(ELSE_ON) case ON:
			passed = on_event( expression, ctx, &marked );
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
		}
		if (( j && passed )) {
			addItem( &stack, i );
			add_item( &stack, marked );
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
				passed = 1; // otherwise we would not be here
				marked = pop_item( &stack );
				i = popListItem( &stack );
				occurrence = i->ptr;
			}
			else goto RETURN;
		}
	}
RETURN:
	freeItem( i );
#ifdef DEBUG
	fprintf( stderr, "operate end\n" );
#endif
	return 1;
}

void
bm_update( BMContext *ctx, int new )
{
	CNDB *db = BMContextDB( ctx );
	db_update( db );
	if ( new ) db_init( db );
}

//===========================================================================
//	in_condition
//===========================================================================
static int
in_condition( char *expression, BMContext *ctx, int *marked )
/*
	Assumption: *marked = 0 to begin with
*/
{
	CNInstance *found;
	int success, negated=0;
#ifdef DEBUG
	fprintf( stderr, "in condition bgn: %s\n", expression );
#endif
	if ( !strncmp( expression, "~.:", 3 ) )
		{ negated=1; expression += 3; }

	switch ( expression[0] ) {
	case ':':
		success = bm_assign_op( IN, expression, ctx, marked );
		goto RETURN;
	case '~':
		switch ( expression[1] ) {
		case '.':
			success = db_is_empty( 0, BMContextDB(ctx) );
			goto RETURN;
		}
		break;
	}
DEFAULT:
	found = bm_feel( expression, ctx, BM_CONDITION );
	success = bm_context_mark( ctx, expression, found, marked );

RETURN:
#ifdef DEBUG
	fprintf( stderr, "in_condition end\n" );
#endif
	return ( negated ? !success : success );
}

//===========================================================================
//	on_event
//===========================================================================
static int
on_event( char *expression, BMContext *ctx, int *marked )
/*
	Assumption: *marked = 0 to begin with
*/
{
	CNInstance *found;
	int success, negated=0;
#ifdef DEBUG
	fprintf( stderr, "on_event bgn: %s\n", expression );
#endif
	if ( !strncmp( expression, "~.:", 3 ) )
		{ negated=1; expression += 3; }

	switch ( expression[0] ) {
	case ':':
		success = bm_assign_op( ON, expression, ctx, marked );
		goto RETURN;
	case '~':
		switch ( expression[1] ) {
		case '(':
			expression++;
			found = bm_feel( expression, ctx, BM_RELEASED );
			success = bm_context_mark( ctx, expression, found, marked );
			goto RETURN;
		case '.':
			success = db_still( BMContextDB(ctx) );
			goto RETURN;
		}
		break;
	default:
		if ( !strcmp( expression, "init" ) ) {
			success = db_in( BMContextDB(ctx) );
			goto RETURN;
		}
	}
DEFAULT:
	found = bm_feel( expression, ctx, BM_MANIFESTED );
	success = bm_context_mark( ctx, expression, found, marked );

RETURN:
#ifdef DEBUG
	fprintf( stderr, "on_event end\n" );
#endif
	return ( negated ? !success : success );
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
	switch ( expression[0] ) {
	case '.':
		goto RETURN;
	case ':':
		bm_assign_op( DO, expression, ctx, NULL );
		goto RETURN;
	case '~':
		switch ( expression[1] ) {
		case '(':
			bm_release( expression+1, ctx );
			goto RETURN;
		case '.':
			goto RETURN;
		}
		break;
	default:
		if ( !strcmp( expression, "exit" ) ) {
			db_exit( BMContextDB(ctx) );
			goto RETURN;
		}
	}
DEFAULT:
	bm_instantiate( expression, ctx );

RETURN:
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
	Assuming expression is in the form e.g.
		args : format <
	reads input from stdin according to format
*/
{
#ifdef DEBUG
	fprintf( stderr, "do_input bgn: %s\n", expression );
#endif
	/* extract args:{ expression(s) } and format
	*/
	listItem *args = NULL;
	CNString *string = newString();
	char *p=expression, *q=p_prune( PRUNE_FILTER, p );
	for ( ; *q==':'; q=p_prune( PRUNE_FILTER, p ) ) {
		for ( char *s=p; s!=q; s++ )
			StringAppend( string, *s );
		char *arg = StringFinish( string, 0 );
		addItem( &args, arg );
		p = q + 1;
		StringReset( string, CNStringMode );
	}
	reorderListItem( &args );
	char *format = p;
	freeString( string );

	/* invoke bm_inputf()
	*/
	int retval = ( *format == '"' ) ?
		bm_inputf( format, args, ctx ) :
		bm_inputf( "", args, ctx );

	/* cleanup
	*/
	while (( p = popListItem( &args ) ))
		free( p );
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
#ifdef DEBUG
	fprintf( stderr, "do_output end\n" );
#endif
	return retval;
}

