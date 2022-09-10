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
//	operate
//===========================================================================
#define ctrl(e)	case e:	if ( passed ) { j = NULL; break; }

static Pair *
operate(
	CNNarrative *narrative, CNInstance *instance, BMContext *ctx,
	listItem *narratives, CNStory *story )
{
#ifdef DEBUG
	fprintf( stderr, "operate bgn\n" );
#endif
	Pair *request = newPair( NULL, NULL );
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
	return request;
}

//===========================================================================
//	bm_operate
//===========================================================================
static void integrate( Registry *subs, Registry *index );

int
bm_operate( CNCell *cell, listItem **new, CNStory *story )
{
	BMContext *ctx = cell->ctx;
	CNDB *db = BMContextDB( ctx );
	if ( db_out(db) ) return 0;

	listItem *	narratives = cell->entry->value;
	CNNarrative *	base = narratives->ptr; // base narrative
	Registry *	base_registry = newVariableRegistry();

	Registry *index[ 2 ], *swap;
	index[ 0 ] = newRegistry( IndexedByAddress );
	index[ 1 ] = newRegistry( IndexedByAddress );
	Pair *entry = registryRegister( index[ 1 ], base, newRegistry(IndexedByAddress) );
	registryRegister((Registry *) entry->value, NULL, base_registry );
	do {
		swap = index[ 0 ];
		index[ 0 ] = index[ 1 ];
		index[ 1 ] = swap;
		Pair *entry[ 2 ];
		while (( entry[ 0 ] = popListItem( &index[ 0 ]->entries ) )) {
			CNNarrative *narrative = entry[ 0 ]->name;
			Registry *instances = entry[ 0 ]->value;
			while (( entry[ 1 ] = popListItem( &instances->entries ) )) {
				CNInstance *instance = entry[ 1 ]->name;
				Registry *registry = entry[ 1 ]->value;
				/* operate [ [ narrative, instance ], registry ]
				*/
				ctx->registry = registry;
				Pair *request = operate( narrative, instance, ctx, narratives, story );
				freeVariableRegistry( registry );
				if (( request->name )) {
					Registry *subs = request->name;
					integrate( subs, index[ 1 ] );
					freeRegistry( subs, NULL );
				}
				freePair( request );
				freePair( entry[ 1 ] );
			}
			freePair( entry[ 0 ] );
		}
	} while (( index[ 1 ]->entries ));
	freeRegistry( index[ 0 ], NULL );
	freeRegistry( index[ 1 ], NULL );
	ctx->registry = NULL;
	return 1;
}
static void
integrate( Registry *subs, Registry *index )
/*
	integrate subs:%{[ narrative, %{[ instance, registry ]}} into
	index of the same type - minus [ narrative, instance ] doublons
	Note: brute force implementation
*/
{
	Pair *entry[ 2 ], *ref;
	while (( entry[ 0 ] = popListItem( &subs->entries ) )) {
		CNNarrative *narrative = entry[ 0 ]->name;
		ref = registryLookup( index, narrative );
		if ( !ref )
			registryRegister( index, narrative, entry[ 0 ]->value );
		else {
			Registry *ref_instances = ref->value;
			Registry *instances = entry[ 0 ]->value;
			while (( entry[ 1 ] = popListItem( &instances->entries ) )) {
				CNInstance *instance = entry[ 1 ]->name;
				Registry *registry = entry[ 1 ]->value;
				if ( !registryLookup( ref_instances, instance ) )
					registryRegister( ref_instances, instance, registry );
				else {
					freeVariableRegistry( registry );
				}
				freePair( entry[ 1 ] );
			}
		}
		freePair( entry[ 0 ] );
	}
}

//===========================================================================
//	bm_update
//===========================================================================
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

