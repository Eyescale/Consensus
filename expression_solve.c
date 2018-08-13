#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <inttypes.h>

#include "list.h"
#include "registry.h"
#include "database.h"
#include "kernel.h"
#include "output.h"

#include "api.h"
#include "output_util.h"
#include "expression.h"
#include "expression_util.h"
#include "expression_solve.h"
#include "expression_sieve.h"
#include "variable.h"
#include "variable_util.h"
#include "xl.h"

/*
	Note: the present implementation supports the possibility to
	return a negative value on error, although it is not used now.
*/

// #define DEBUG
#define TEST_MEMORY_LEAKS

#ifdef TEST_MEMORY_LEAKS
static void
test_memory_leaks( Expression *expression )
{
	if ( expression == NULL ) return;
	ExpressionSub *sub = expression->sub;
	for ( int i=0; i<4; i++ ) {
		if (( sub[ i ].result.list ))
			outputf( Debug, "***** test_memory_leaks: found sub[ %d ].result.list", i );
		if ( !sub[ i ].result.any )
			test_memory_leaks( sub[ i ].e );
	}
	if (( expression->result.list ))
		output( Debug, "***** test_memory_leaks: found expression->result.list" );
}
#endif

/*---------------------------------------------------------------------------
	expression_solve
---------------------------------------------------------------------------*/
static ExpressionMode init_mode( Expression *, int as_sub, listItem *filter, _context * );

static int solve( Expression *, int as_sub, listItem *filter, _context * );
static int resolve( Expression *, _context * );
static void sort_results( Expression *, listItem ** );

int
expression_solve( Expression *expression, int as_sub, listItem *filter, _context *context )
/*
	First, expression_solve() invokes solve() or sieve(), depending on the mode,
	to find, extract and pass results top-down from the leaves to the root of
	the expression graph. Then it invokes resolve() to extract the sub-results
	corresponding to the node marked '?', parsing the graph from bottom up.
*/
{
#ifdef DEBUG
	outputf( Debug, ">>>>> entering expression_solve: mode=%d, expression=%0x, filter=%0x",
		context->expression.mode, (int) expression, (int) filter );
#if 0
	output_expression( ExpressionAll, expression );
	output( Text, "\n" );
#endif
#endif
	freeListItem( &context->expression.results );
	if ( expression == NULL ) return 0;

	// ward against infinite recursion - embedded expressions
	// ------------------------------------------------------
	if (( lookupItem( context->expression.stack, expression ) )) {
		return output( Error, "recursion in expression" );
	}
	addItem( &context->expression.stack, expression );

	// set current and restore mode depending on filter value
	// ------------------------------------------------------
	int restore_mode = context->expression.mode;
	context->expression.mode = init_mode( expression, as_sub, filter, context );
	if ( context->expression.mode == ErrorMode )
		return -1;

	// extract general expression results - i.e. as if no question mark
	// ----------------------------------------------------------------
#ifdef DEBUG
	outputf( Debug, "expression_solve: mode: current=%d, restore=%d, filter=%0x",
		context->expression.mode, restore_mode, (int)filter );
#endif
	// if context->expression.mode is null, context->expression.filter will hold
	// self-generated filter (i.e. literals extracted from strings), if there is
	listItem *restore_filter = context->expression.filter;
	context->expression.filter = NULL;

	int success = ( context->expression.mode == ReadMode ) ?
		sieve( expression, as_sub, filter, context ) :
		solve( expression, as_sub, filter, context );
#ifdef DEBUG
	outputf( Debug, "expression_solve: solved: success=%d, mode=%d", success, context->expression.mode );
#endif

	// invoke resolve() to extract marked results
	// ------------------------------------------
	// Note that resolve must be called even in TestMode due to embedded expressions
	if ( success > 0 ) {
		if ( expression->result.list == filter ) {
			listItem *results = NULL;
			for ( listItem *i = expression->result.list; i!=NULL; i=i->next )
				addItem( &results, i->ptr );
			expression->result.list = results;
		}
		else if ( expression->result.list == context->expression.filter )
			context->expression.filter = NULL;

		if ( expression->result.marked ) {
			resolve( expression, context );
		}
		if ( context->expression.mode == ReadMode )
		/*
			results are references to expression-subs of the original filter
			values. We need to turn them into standalone literals, removing
			literal redundancy (doublons) as we go.
		*/
		{
			listItem *results = expression->result.list;
			listItem *last_i = NULL, *next_i, *j;
			for ( listItem *i = results; i!=NULL; i=next_i ) {
				next_i = i->next;
				for ( j = results; j!=i; j=j->next )
					if ( !lcmp( i->ptr, j->ptr ) )
						break;
				if ( j == i ) {
					i->ptr = ldup( i->ptr );
					last_i = i;
				}
				else clipListItem( &results, i, last_i, next_i );
			}
			expression->result.list = results;
		}
		success = ( expression->result.list != NULL );
	}
	freeLiterals( &context->expression.filter );
	context->expression.filter = restore_filter;

	// translate results according to current and restore mode
	// -------------------------------------------------------
	if (( success > 0 ) && restore_mode ) {
		switch ( restore_mode ) {
		case InstantiateMode:
		case EvaluateMode:
			if ( context->expression.mode == ReadMode )
				success = L2E( &expression->result.list, filter, context );
			break;
		case ReadMode:
			if ( context->expression.mode == EvaluateMode )
			 	success = E2L( &expression->result.list, filter, context );
			break;
		default:
			break;
		}
		context->expression.mode = restore_mode;
	}
	context->expression.results = expression->result.list;
	expression->result.list = NULL;

	// pop expression from expression stack
	// ------------------------------------
	popListItem( &context->expression.stack );

	if (( success > 0 ) && ( context->expression.mode == EvaluateMode ))
		sort_results( expression, &context->expression.results );

#ifdef TEST_MEMORY_LEAKS
	test_memory_leaks( expression );
#endif
#ifdef DEBUG
	outputf( Debug, "<<<<< exiting expression_solve - success=%d, mode=%d", success, context->expression.mode );
#endif
	return success;
}

static int sortable( Expression * );
static void
sort_results( Expression *expression, listItem **results )
{
	if ( !sortable( expression ) ) return;
	int mark = 0;
	Registry *registry = newRegistry( IndexedByName );
	for ( listItem *i = *results; i!=NULL; i=i->next ) {
		Entity *e = i->ptr;
		if ( e != CN.nil ) {
			registryRegister( registry, cn_name(e), e );
		}
		else mark = 1;
	}
	freeListItem( results );
	if ( mark ) addItem( results, CN.nil );
	for ( registryEntry *r = registry->value; r!=NULL; r=r->next )
		addItem( results, r->value );
	freeRegistry( &registry );
	reorderListItem( results );
}
static int
sortable( Expression *expression )
{
	ExpressionSub *sub = expression->sub;
	if (( sub[ 3 ].e )) {
		return ( sub[ 3 ].result.not && just_blank( sub[ 3 ].e )) ?
			1 : sortable( sub[ 3 ].e );
	}
	else if ( sub[ 3 ].result.any ) {
		if ( sub[ 1 ].result.none && ( sub[ 0 ].e ) ) {
			return ( sub[ 0 ].result.not && just_blank( sub[ 0 ].e )) ?
				1 : sortable( sub[ 0 ].e );
		}
	}
	return 0;
}

static int instantiable( Expression *, _context * );
static ExpressionMode
init_mode( Expression *expression, int as_sub, listItem *filter, _context *context )
{
	// test if this is a literal which is passed as expression
	if ( expression->sub[ 3 ].e == expression )
		return context->expression.mode;

	switch ( context->expression.mode ) {
	case InstantiateMode:
		return instantiable( expression, context ) ? InstantiateMode : ErrorMode;
	case ReadMode:
		if ( as_sub != 3 ) {
			outputf( Debug, "***** Error: expression_solve: as_sub!=3 (ReadMode)" );
			return ErrorMode;
		}
		return ( filter == NULL ) ? 0 : ReadMode;
	case EvaluateMode:
		return ( filter == NULL ) ? 0 : EvaluateMode;
	default:
		if (( filter )) {
			outputf( Error, "expression_solve: cannot determine filter type" );
			return ErrorMode;
		}
		return context->expression.mode;
	}
}
static int
instantiable( Expression *expression, _context *context )
/*
	DO_LATER: instantiability can and should be tested on the go -
		  see comments in solve() below
*/
{
	if ( expression->result.marked ) {
		output( Error, "'?' is not instantiable" );
		return 0;
	}
	ExpressionSub *sub = expression->sub;
	if ( expression->result.as_sub || !sub[ 3 ].result.none ) {
		output( Error, "instance designation is not allowed in instantiation mode" );
		return 0;
	}
	if ( sub[ 0 ].result.any || sub[ 1 ].result.any || sub[ 2 ].result.any ) {
		output( Error, "'.' is not instantiable" );
		return 0;
	}
	for ( int i=0; i<4; i++ ) {
		if ( flagged( expression, i ) ) {
			output( Error, "expression flags are not allowed in instantiation mode" );
			return 0;
		}
	}
	for ( int i=0; i<3; i++ ) {
		if ( sub[ i ].result.any ) continue;
		if ( sub[ i ].e ) {
			if ( !instantiable( sub[ i ].e, context ) )
				return 0;
		}
		else switch ( sub[ i ].result.identifier.type ) {
		case VariableIdentifier:
			if (( context->expression.args ))
				break;

			char *identifier = sub[ i ].result.identifier.value;
			VariableVA *variable = lookupVariable( context, identifier );
			if ( variable == NULL ) {
				outputf( Error, "instantiable: variable '%s' not found", identifier );
				return 0;
			}
			switch ( variable->type ) {
			case EntityVariable:
			case LiteralVariable:
				break;
			case StringVariable:
				outputf( Error, "'%s' is a string variable - not instantiable", identifier );
				return 0;
			case NarrativeVariable:
				outputf( Error, "'%s' is a narrative variable - "
					"not allowed in expressions", identifier );
				return 0;
			case ExpressionVariable:
				if ( !instantiable( ((listItem * ) variable->value )->ptr, context ) )
					return 0;
				break;
			}
			break;
		default:
			break;
		}
	}
	return 1;
}

/*---------------------------------------------------------------------------
	solve
---------------------------------------------------------------------------*/
static int sub_solve( Expression *, int count, int as_sub, listItem *filter, _context * );
static int sub_eval( Expression *, int count, int as_sub, _context *context );

int set_active2( Expression *expression, int *not );
int set_as_sub2( Expression *expression, int as_sub );

static int test_as_sub( Entity *e, int as_sub2 );
static int test_active( Entity *e, int active2 );

static listItem *take_not( ExpressionSub *, int as_sub2, int active2, listItem *filter );
static listItem *take_sub( ExpressionSub *, int as_sub2, int active2, listItem *filter );
static listItem *take_all( Expression *, int as_sub2, int active2, listItem *filter );
static listItem *take_sup( Expression *, int as_sup );

static int
solve( Expression *expression, int as_sub, listItem *filter, _context *context )
{
#ifdef DEBUG
	outputf( Debug, "solve: entering - %0x", (int) expression );
#if 0
	output_expression( ExpressionAll, expression );
	output( Text, "\n" );
#endif
#endif
	ExpressionSub *sub = expression->sub;
	listItem *sub_filter = NULL;
	int success = 0;

	// initialize test flags
	// ---------------------
	int any[ 4 ], not[ 4 ];
	for ( int count=0; count<4; count++ ) {
		any[ count ] = sub[ count ].result.any;
		not[ count ] = sub[ count ].result.not;
	}

	// set sub_filter to filtered instance results
	// -------------------------------------------
	if ( context->expression.mode == ReadMode )
	/*
		actually should never happen - cf. expression_solve()
	*/
	{
		return sieve( expression, as_sub, filter, context );
	}
	else if (( filter )) {
		success = sub_solve( expression, 3, as_sub, filter, context );
		if ( success > 0 ) {
			sub_filter = sub[ 3 ].result.list;
			sub[ 3 ].result.list = NULL;
		}
		else return success;
	}
	else if ( !any[ 3 ] && !sub[ 3 ].result.none )
	/*
		no filter - extract instance values directly from CNDB
		the returned values are tested against active, passive
		and as_sub, but not negated.
	*/
	{
		success = sub_eval( expression, 3, as_sub, context );
		if (( success < 0 ) || (( success == 0 ) && !not[ 3 ] ))
			return success;
		/*
			Note here that the mode may have been set to ReadMode - if
			instance is not flagged in any way and its value is literals
		*/
		if ( success == 0 ) {
			// not nothing is anything
			not[ 3 ] = 0;
			any[ 3 ] = 1;
		}
		else if ( !not[ 3 ] ) {
			sub_filter = sub[ 3 ].result.list;
			sub[ 3 ].result.list = NULL;
		}
	}
	if (( context->expression.mode == 0 ) && !sub[ 3 ].result.none )
		context->expression.mode = EvaluateMode;

	// extract other expression terms
	// ------------------------------
	if ( sub[ 1 ].result.none )
	/*
		special case: expression has no medium nor target term
	*/
	{
		if ( context->expression.mode == ReadMode ) {
			success = sub_sieve( expression, 0, 3, sub_filter, context );
			if ( success > 0 ) {
				expression->result.list = sub[ 0 ].result.list;
				sub[ 0 ].result.list = NULL;
			}
		}
		else if (( sub_filter )) {
			success = sub_solve( expression, 0, 3, sub_filter, context );
			if ( success > 0 ) {
				expression->result.list = sub[ 0 ].result.list;
				sub[ 0 ].result.list = NULL;
			}
		}
		else
		/*
			from here on both filter and sub_filter are NULL, and
			the instance term of the expression is either any or,
			if it holds results, those are negated.
			Otherwise we would have had a sub_filter.
		*/
		if ( any[ 0 ] ) {
			if ( !context->expression.mode ) {
				context->expression.mode = EvaluateMode;
			}
			int active2 = set_active2( expression, not );
			int as_sub2 = set_as_sub2( expression, as_sub );
			expression->result.list = not[ 3 ] ?
				take_not( &sub[ 3 ], as_sub2, active2, NULL ) :
				take_all( expression, as_sub2, active2, NULL );
		}
		else
		/*
			fill in source results
		*/
		{
			success = sub_eval( expression, 0, as_sub, context );
			if (( success < 0 ) || (( success == 0 ) && !not[ 0 ] ))
				goto RETURN_SUCCESS;
			/*
			   Note here that the mode may have been set to ReadMode - if
			   1) instance is none, and 2) source is not flagged in any way
			   and its value is literals
			*/
			if ( success == 0 ) {
				// not nothing is anything
				not[ 0 ] = 0;
				any[ 0 ] = 1;
			}
			int active2 = set_active2( expression, not );
			int as_sub2 = set_as_sub2( expression, as_sub );
			listItem *results;
			if ( context->expression.mode == ReadMode ) {
				results = sub[ 0 ].result.list;
				sub[ 0 ].result.list = NULL;
			}
			else if ( any[ 0 ] ) {
				results = not[ 3 ] ?
					take_not( &sub[ 3 ], as_sub2, active2, NULL ) :
					take_all( expression, as_sub2, active2, NULL );
			}
			else if ( any[ 3 ] ) {
				results = not[ 0 ] ?
					take_not( &sub[ 0 ], as_sub2, active2, NULL ) :
					take_sub( &sub[ 0 ], as_sub2, active2, NULL );
			}
			else {
				results = not[ 3 ] ?
					take_not( &sub[ 3 ], as_sub2, active2, NULL ) :
					take_sub( &sub[ 3 ], as_sub2, active2, NULL );
				results = not[ 0 ] ?
					take_not( &sub[ 0 ], 0, 0, results ) :
					take_sub( &sub[ 0 ], 0, 0, results );
			}
			expression->result.list = results;
		}
	}
	else
	/*
	   general case(s): expression has source, medium and target terms
	   ---------------------------------------------------------------
	*/
	if ( context->expression.mode == ReadMode ) {
		// extract expression terms from sub_filter
		for ( int count=0; count<3; count++ ) {
			success = sub_sieve( expression, count, 3, sub_filter, context );
			if ( success <= 0 ) goto RETURN_SUCCESS;
		}

		// set expression results to those filter values whose subs passed screening
		int count;
		for ( listItem *i = sub_filter; i!=NULL; i=i->next ) {
			Literal *l = i->ptr;
			for ( count=0; count<3; count++ )
				if ( !lookupItem( sub[ count ].result.list, l_sub( l, count ) ) )
					break;
			if ( count == 3 ) addItem( &expression->result.list, l );
		}
	}
	else if (( sub_filter )) {
		// extract expression terms from sub_filter
		for ( int count=0; count<3; count++ ) {
			success = sub_solve( expression, count, 3, sub_filter, context );
			if ( success <= 0 ) goto RETURN_SUCCESS;
		}

		// set expression results to those filter values whose subs passed screening
		int count;
		for ( listItem *i = sub_filter; i!=NULL; i=i->next ) {
			Entity *e = i->ptr;
			for ( count=0; count<3; count++ )
				if ( !lookupItem( sub[ count ].result.list, e->sub[ count ] ) )
					break;
			if ( count == 3 ) addItem( &expression->result.list, e );
		}
	}
	else if ( context->expression.mode == InstantiateMode )
	/*
		special case: instantiation ( no filter, sub_filter nor instance )

		DO_LATER: today the following possibilities are ruled out by instantiable(), whereas
			. where no sub-result, could instantiate with Nil
			. where sub_result is any, could instantiate with whole CNDB
			. where sub-result is negated, could instantiate with inverted sub-result
			  possibly including the whole CNDB
			. where sub-result is flagged, could instantiate with tested sub-result
	*/
	{
		for ( int count=0; count<3; count++ ) {
			success = sub_eval( expression, count, 3, context );
			if ( success <= 0 ) goto RETURN_SUCCESS;
		}
		for ( listItem *i = sub[ 0 ].result.list; i!=NULL; i=i->next )
		for ( listItem *j = sub[ 1 ].result.list; j!=NULL; j=j->next )
		for ( listItem *k = sub[ 2 ].result.list; k!=NULL; k=k->next )
		{
			Entity *source = (Entity *) i->ptr;
			Entity *medium = (Entity *) j->ptr;
			Entity *target = (Entity *) k->ptr;
			Entity *e = cn_instance( source, medium, target );
			if ( e == NULL ) {
				e = cn_instantiate( source, medium, target, expression->result.twist );
#if 0
				outputf( Debug, "solver: created new relationship instance: ( %0x, %0x, %0x ): %0x",
					(int) source, (int) medium, (int) target, (int) e );
#endif
			}
			if (( e )) addItem( &expression->result.list, e );
		}
	}
	else
	/*
		from here on both filter and sub_filter are NULL, and the instance term of
		the expression is either any or, if it holds results, those are negated.
		Otherwise we would have had a sub_filter.
	*/
	{
		// fill in expression term results
		// -------------------------------
		for ( int count=0; count<3; count++ ) {
			if ( any[ count ] ) continue;

			success = sub_eval( expression, count, count, context );
			if (( success < 0 ) || (( success == 0 ) && !not[ count ] ))
				goto RETURN_SUCCESS;
			else if ( success == 0 ) {
				// not nothing is anything
				not[ count ] = 0;
				any[ count ] = 1;
			}
		}

		// set active and as_sub bitwise test combinations
		int active2 = set_active2( expression, not );
		int as_sub2 = set_as_sub2( expression, as_sub );

		// special case regarding the '~' and '.' flags
		// --------------------------------------------
		if (( any[ 0 ] || not[ 0 ] ) && ( any[ 1 ] || not[ 1 ] ) &&
		    ( any[ 2 ] || not[ 2 ] ) && ( any[ 3 ] || not[ 3 ] ))
		/*
		   in this case we would not be able to get a first candidate, so we must perform
		   the inversion or extraction over the whole database.
		*/
		{
			int count = ( any[ 0 ] ? any[ 1 ] ? any[ 2 ] ? any[ 3 ] ? 4 : 3 : 2 : 1 : 0 );
			switch ( count ) {
			case 4:
				expression->result.list = take_all( expression, as_sub2, active2, NULL );
				goto RETURN_RESULTS;
			case 3:
				expression->result.list = take_not( &sub[ 3 ], as_sub2, active2, NULL );
				goto RETURN_RESULTS;
			default:
				not[ count ] = 0;
				sub[ count ].result.list = take_not( &sub[ count ], ( 1 << count ), 0, NULL );
			}
		}

		// generate expression results
		// ---------------------------
#ifdef DEBUG
		output( Debug, "solver: 4. searching..." );
#endif
		listItem *source = sub[ 0 ].result.list;
		listItem *medium = sub[ 1 ].result.list;
		listItem *target = sub[ 2 ].result.list;
		listItem *instance = sub[ 3 ].result.list;
		listItem *candidate =
			(( instance ) && !not[ 3 ] ) ? instance :
			(( target ) && !not[ 2 ] ) ? ((Entity *) target->ptr )->as_sub[ 2 ] :
			(( source ) && !not[ 0 ] ) ? ((Entity *) source->ptr )->as_sub[ 0 ] :
			(( medium ) && !not[ 1 ] ) ? ((Entity *) medium->ptr )->as_sub[ 1 ] :
			NULL;

		if ( candidate == NULL ) {
#ifdef DEBUG
			output( Debug, "no candidate" );
#endif
			goto RETURN_RESULTS;
		}
#ifdef DEBUG
		outputf( Debug, " - %0x", (int) candidate->ptr );
#endif
		// execute loop
		for ( ; ; ) {

			Entity *e = candidate->ptr;
			if ( test_as_sub( e, as_sub2 ) && test_active( e, active2 ) &&
			     ( any[ 0 ] || ( not[ 0 ] ? ( e->sub[ 0 ] != source->ptr ) : ( e->sub[ 0 ] == source->ptr ) )) &&
			     ( any[ 1 ] || ( not[ 1 ] ? ( e->sub[ 1 ] != medium->ptr ) : ( e->sub[ 1 ] == medium->ptr ) )) &&
			     ( any[ 2 ] || ( not[ 2 ] ? ( e->sub[ 2 ] != target->ptr ) : ( e->sub[ 2 ] == target->ptr ) )) &&
			     ( any[ 3 ] || ( not[ 3 ] ? ( e != instance->ptr ) : ( e == instance->ptr ) )) )
			{
#ifdef DEBUG
				output( Debug, "solver: good one - calling addIfNotThere" );
#endif
				// addIfNotThere eliminates doublons and sorts the list
				addIfNotThere( &expression->result.list, candidate->ptr );
			}
#ifdef DEBUG
			 else output( Debug, "solver: wrong candidate" );
#endif
			if ( candidate->next != NULL ) {
				candidate = candidate->next;
			}
			else do {
				if ( !any[ 3 ] && ( instance->next )) {
					instance = instance->next;
				}
				else if ( !any[ 2 ] && ( target->next )) {
					instance = sub[ 3 ].result.list;
					target = target->next;
				}
				else if ( !any[ 1 ] && ( medium->next )) {
					instance = sub[ 3 ].result.list;
					target = sub[ 2 ].result.list;
					medium = medium->next;
				}
				else if ( !any[ 0 ] && ( source->next )) {
					instance = sub[ 3 ].result.list;
					target = sub[ 2 ].result.list;
					medium = sub[ 1 ].result.list;
					source = source->next;
				}
				else {
#ifdef DEBUG
					output( Debug, "solver: returning" );
#endif
					goto RETURN_RESULTS;
				}
				candidate =
					(( instance ) && !not[ 3 ] ) ? instance :
					(( target ) && !not[ 2 ] ) ? ((Entity *) target->ptr )->as_sub[ 2 ] :
					(( source ) && !not[ 0 ] ) ? ((Entity *) source->ptr )->as_sub[ 0 ] :
					(( medium ) && !not[ 1 ] ) ? ((Entity *) medium->ptr )->as_sub[ 1 ] :
					NULL;
			}
			while( candidate == NULL );
		}
	}
RETURN_RESULTS:
	success = ( expression->result.list != NULL );
RETURN_SUCCESS:
	// cleanup sub-filter
	if (( expression->result.list != sub_filter ) && ( sub_filter != filter ))
		freeListItem( &sub_filter );
	// cleanup sub-results
	for ( int count=0; count<4; count++ )
		freeListItem( &sub[ count ].result.list );

	// take sup if required
	if (( success > 0 ) && expression->result.as_sup ) {
		int as_sup = expression->result.as_sup;
		expression->result.list = ( context->expression.mode == ReadMode ) ?
			l_take_sup( expression, as_sup ) :
			take_sup( expression, as_sup );
	}
	else return success;

	return ( expression->result.list != NULL );
}

/*---------------------------------------------------------------------------
	sub_solve
---------------------------------------------------------------------------*/
static int e_test( Entity *e, int as_sub2, int active, int passive );
static listItem *sub_solve_expressions( listItem *expressions, listItem *filter, _context * );

int
sub_solve( Expression *expression, int count, int as_sub, listItem *filter, _context *context )
/*
	Assumptions: filter is not null, context->expression.mode is EvaluateMode
*/
{
	void *value;
	ExpressionSub *sub = expression->sub;
	listItem **sub_results = &sub[ count ].result.list;

	int active = sub[ count ].result.active;
	int passive = sub[ count ].result.passive;
	int not = sub[ count ].result.not;

	// extract sub-filter
	// ------------------
	listItem *sub_filter = NULL;
	if (( count == 3 ) || sub[ 1 ].result.none ) {
		int as_sub2 = set_as_sub2( expression, as_sub );
		if ( as_sub2 || not || active || passive ) {
			for ( listItem *i = filter; i!=NULL; i=i->next ) {
				Entity *e = i->ptr;
				if ( e_test( e, as_sub2, active, passive ) ) {
					if ( !not ) addItem( &sub_filter, e );
				}
				else if ( not ) addItem( &sub_filter, e );
			}
		}
		else sub_filter = filter;
	}
	else {
		for ( listItem *i = filter; i!=NULL; i=i->next ) {
			Entity *e = ((Entity *) i->ptr )->sub[ count ];
			if ( e == NULL ) continue;
			if (( !active || cn_is_active( e ) ) && ( !passive || !cn_is_active( e ) )) {
				 if ( !not ) addIfNotThere( &sub_filter, e );
			}
			else if ( not ) addIfNotThere( &sub_filter, e );
		}
	}

	if ( sub[ count ].result.any ) {
		*sub_results = sub_filter;
		return ( *sub_results != NULL );
	}
	else if ( sub_filter == NULL )
		return 0;

	// screen expression term values through sub-filter
	// ------------------------------------------------
	switch ( sub_type( expression, count, &value, context ) ) {
	case IdentifierValueType:
		; Entity *e = cn_entity( value );
		if (( e ) && (lookupItem( sub_filter, e )))
			addItem( sub_results, e );
		break;
	case EntityValueType:
		if (( value ) && (lookupItem( sub_filter, value )))
			addItem( sub_results, value );
		break;
	case EntitiesValueType:
		for ( listItem *i = value; i!=NULL; i=i->next ) {
			Entity *e = i->ptr;
			if (( e ) && (lookupItem( sub_filter, e )))
				addItem( sub_results, i->ptr );
		}
		break;
	case LiteralsValueType:
		for ( listItem *i = value; i!=NULL; i=i->next ) {
			Entity *e = l2e( i->ptr );
			if (( e ) && (lookupItem( sub_filter, e )))
				addIfNotThere( sub_results, e );
		}
		break;
	case StringsValueType:
		// in case of embedded expressions: ward against infinite recursion
		if ( lookupItem( context->expression.stack, value ) )
			return output( Error, "recursion in expression: on string variable" );
		listItem *expressions = S2X( value, NULL, context );
		if (( expressions )) {
			addItem( &context->expression.stack, value );
			*sub_results = sub_solve_expressions( expressions, sub_filter, context );
			freeExpressions( &expressions );
			popListItem( &context->expression.stack );
		}
		break;
	case ExpressionsValueType:
		*sub_results = sub_solve_expressions( value, sub_filter, context );
		break;
	case SubValueType:
		; Expression *sub_e = value;
		int success = solve( sub_e, 3, sub_filter, context );
		if ( success > 0 )
		/*
			Assumption: sub_filter cannot be returned - cf. expression_collapse()
		*/
		{
			*sub_results = sub_e->result.list;
			sub_e->result.list = NULL;
		}
		break;
	default: // ErrorValueType
		if ( sub_filter != filter ) freeListItem( &sub_filter );
		return 0;
	}

	if ( sub_filter != filter ) freeListItem( &sub_filter );
	return ( *sub_results != NULL );
}

static int
e_test( Entity *e, int as_sub2, int active, int passive )
{
	return ( test_as_sub( e, as_sub2 ) && ( !active || cn_is_active( e ) ) && ( !passive || !cn_is_active( e ) ));
}
static listItem *
sub_solve_expressions( listItem *expressions, listItem *filter, _context *context )
{
	listItem *results = NULL;
	for ( listItem *i = expressions; i!=NULL; i=i->next ) {
		int success = expression_solve( i->ptr, 3, filter, context );
		if ( success > 0 ) {
			for ( listItem *j = context->expression.results; j!=NULL; j=j->next ) {
				addIfNotThere( &results, j->ptr );
			}
			freeListItem( &context->expression.results );
		}
	}
	return results;
}

/*---------------------------------------------------------------------------
	sub_eval
---------------------------------------------------------------------------*/
static int test_instantiate( Expression *, int count, _context * );
static int test_read( Expression *, int count, _context * );
static listItem *e_clip( listItem **list, int active, int passive );
static listItem *sub_eval_expressions( listItem *, int as_sub, int active, int passive, _context * );

static int
sub_eval( Expression *expression, int count, int as_sub, _context *context )
/*
	Results are tested against as_sub, active and passive, but not negated
*/
{
	void *value;
	ExpressionSub *sub = expression->sub;
	listItem **sub_results = &sub[ count ].result.list;

	int as_sub2 = (( count == 3 ) || sub[ 1 ].result.none ) ?
		set_as_sub2( expression, as_sub ) : ( 1 << as_sub );
	int active = sub[ count ].result.active;
	int passive = sub[ count ].result.passive;

	switch ( sub_type( expression, count, &value, context ) ) {
	case IdentifierValueType:
		if ( test_instantiate( expression, count, context ) )
		{
			Entity *e = cn_entity( value );
			if ( e == NULL )
				e = cn_new( strdup( value ) );
			if (( e )) addItem( sub_results, e );
#ifdef DEBUG
			outputf( Debug, "solve[ %d ]: created new: '%s', addr: %0x", count, value, (int) e );
#endif
		}
		else {
			Entity *e = cn_entity( value );
			if (( e ) && e_test( e, as_sub2, active, passive ))
				addItem( sub_results, e );
		}
		break;
	case EntityValueType:
		if ( e_test( value, as_sub2, active, passive ) )
			addItem( sub_results, value );
		break;
	case EntitiesValueType:
		for ( listItem *i = value; i!=NULL; i=i->next )
			if ( e_test( i->ptr, as_sub2, active, passive ) )
				addItem( sub_results, i->ptr );
		break;
	case LiteralsValueType:
		if ( test_instantiate( expression, count, context ) ) {
			for ( listItem *i = value; i!=NULL; i=i->next ) {
				Entity *e = l_inst( i->ptr );
				if (( e )) addItem( sub_results, e );
			}
		}
		else if ( test_read( expression, count, context ) && !as_sub2 )
		/*
			The mode is set here to ReadMode
		*/
		{
			context->expression.mode = ReadMode;
			for ( listItem *i = value; i!=NULL; i=i->next )
				addItem( sub_results, i->ptr );
		}
		else {
			for ( listItem *i = value; i!=NULL; i=i->next ) {
				Entity *e = l2e( i->ptr );
				if (( e ) && e_test( e, as_sub2, active, passive ))
					addItem( sub_results, e );
			}
		}
		break;
	case StringsValueType:
		// in case of embedded expressions: ward against infinite recursion
		if ( lookupItem( context->expression.stack, value ) )
			return output( Error, "recursion in expression: on string variable" );
		int type;
		listItem *expressions = S2X( value, &type, context );
		if ( type == LiteralResults ) {
			if ( test_read( expression, count, context ) && !as_sub2 )
			/*
				The mode is set here to ReadMode
			*/
			{
				context->expression.mode = ReadMode;
				*sub_results = expressions;
				// save the generated list of literals, to be freed upon
				// return by expression_solve()
				listItem **filter = &context->expression.filter;
				for ( listItem *i = expressions; i!=NULL; i=i->next )
					addItem( filter, i->ptr );
			}
			else {
				for ( listItem *i = expressions; i!=NULL; i=i->next ) {
					Literal *l = i->ptr;
					int success = expression_solve( l->e, as_sub, NULL, context );
					if ( success > 0 ) {
						e_clip( &context->expression.results, active, passive );
						for ( listItem *j = context->expression.results; j!=NULL; j=j->next )
							addIfNotThere( sub_results, j->ptr );
						freeListItem( &context->expression.results );
					}
				}
				freeLiterals( &expressions );
			}
		}
		else if (( expressions ))
		{
			if ( !context->expression.mode ) {
				context->expression.mode = EvaluateMode;
			}
			addItem( &context->expression.stack, value );
			*sub_results = sub_eval_expressions( expressions, as_sub, active, passive, context );
			freeExpressions( &expressions );
			popListItem( &context->expression.stack );
		}
		break;
	case ExpressionsValueType:
		if ( test_read( expression, count, context ) && !as_sub2 ) {
			listItem *literals = Xt2L( value, context );
			if (( literals ))
			/*
				The mode is set here to ReadMode
			*/
			{
				context->expression.mode = ReadMode;
				*sub_results = literals;
				// save the generated list of literals, to be freed upon
				// return by expression_solve()
				listItem **filter = &context->expression.filter;
				for ( listItem *i = literals; i!=NULL; i=i->next )
					addItem( filter, i->ptr );
				break;
			}
		}
		if ( !context->expression.mode ) {
			context->expression.mode = EvaluateMode;
		}
		*sub_results = sub_eval_expressions( value, as_sub, active, passive, context );
		break;
	case SubValueType:
		; Expression *sub_e = value;
		int success = solve( sub_e, as_sub, NULL, context );
		if ( success > 0 ) {
			*sub_results = e_clip( &sub_e->result.list, active, passive );
			sub_e->result.list = NULL;
		}
		break;
	default: // ErrorValueType
		return 0;
	}
	if ( !context->expression.mode ) {
		context->expression.mode = EvaluateMode;
	}
	return ( *sub_results != NULL );
}

static int
test_instantiate( Expression *expression, int count, _context *context )
{
	ExpressionSub *sub = expression->sub;
	return ( context->expression.mode == InstantiateMode ) && sub[ 3 ].result.none && !flagged( expression, count );
}
static int
test_read( Expression *expression, int count, _context *context )
{
	ExpressionSub *sub = expression->sub;
	return (( context->expression.mode == 0 ) && !expression->result.as_sup &&
		    (( count == 3 ) || sub[ 1 ].result.none ) && !flagged( expression, count ));
}
static listItem *
e_clip( listItem **list, int active, int passive )
{
	if ( !active && !passive )
		return *list;

	listItem *last_i = NULL, *next_i;
	for ( listItem *i = *list; i!=NULL; i=next_i ) {
		next_i = i->next;
		if (( active && !cn_is_active( i->ptr )) || ( passive && cn_is_active( i->ptr )))
		{
			clipListItem( list, i, last_i, next_i );
		}
		else last_i = i;
	}
	return *list;
}
static listItem *
sub_eval_expressions( listItem *expressions, int as_sub, int active, int passive, _context *context )
{
	listItem *results = NULL;
	for ( listItem *i = expressions; i!=NULL; i=i->next ) {
		int success = expression_solve( i->ptr, as_sub, NULL, context );
		if ( success > 0 ) {
			e_clip( &context->expression.results, active, passive );
			for ( listItem *j = context->expression.results; j!=NULL; j=j->next )
				addIfNotThere( &results, j->ptr );
			freeListItem( &context->expression.results );
		}
	}
	return results;
}

/*---------------------------------------------------------------------------
	sub_type
---------------------------------------------------------------------------*/
int
sub_type( Expression *expression, int count, void **value, _context *context )
{
	*value = expression->sub[ count ].result.identifier.value;
	switch ( expression->sub[ count ].result.identifier.type ) {
	case NullIdentifier:
		*value = CN.nil;
		return EntityValueType;
	case VariableIdentifier:
		// special cn_setf() support - cf explanations in api.c
		if (( context->expression.args ))
		{
			char *identifier = *value;
			expression->sub[ count ].result.identifier.value = NULL;

			// better have exactly the right arg count, here - otherwise
			// "unpredictable results will happen"!
			listItem *arg = context->expression.args;
			char *fmt;
			long ndx = strtol( identifier, &fmt, 10 );
			while ( ndx-- && (arg)) arg = arg->next;
			if ( arg == NULL ) return -1;

			*value = arg->ptr;
			switch ( *fmt ) {
			case 's':
				return IdentifierValueType;
			case 'e':
				return EntityValueType;
			default:
				return output( Debug, "***** Error: expression_solve: unsupported format" );
			}
		}
		else if (( *value ))
		{
			char *identifier = *value;
			VariableVA *variable = lookupVariable( context, identifier );
			if ( variable == NULL ) {
#ifdef DEBUG
				outputf( Error, "expression_solve: variable '%s' not found", identifier );
#endif
				return 0;
			}
			*value = getVariableValue( variable, context, 0 );
			switch ( variable->type ) {
			case EntityVariable:
				return EntitiesValueType;
			case ExpressionVariable:
				return ExpressionsValueType;
			case LiteralVariable:
				return LiteralsValueType;
			case StringVariable:
				return StringsValueType;
			case NarrativeVariable:
				return outputf( Error, "'%%%s' is a narrative variable - not allowed in expressions", identifier );
			}
		}
		else {
#ifdef DEBUG
			output( Error, "expression terms are incomplete" );
#endif
			return 0;
		}
		break;
	case ExpressionCollect:
		return ExpressionsValueType;
	case LiteralResults:
		return LiteralsValueType;
	case EntityResults:
		return EntitiesValueType;
	case StringResults:
		return StringsValueType;
	case DefaultIdentifier:
		if ( *value == NULL ) {
#ifdef DEBUG
			output( Error, "expression terms are incomplete" );
#endif
			return 0;
		}
		return IdentifierValueType;
	default:
		*value = expression->sub[ count ].e;
		if ( *value == NULL ) {
#ifdef DEBUG
			output( Error, "expression terms are incomplete" );
#endif
			return 0;
		}
		return SubValueType;
	}
}

/*---------------------------------------------------------------------------
	resolve
---------------------------------------------------------------------------*/
static void extract_sub( listItem **sub_results, Expression *, int count, _context * );

static int
resolve( Expression *expression, _context *context )
{
	if ( expression->result.list == NULL )
		return 0;
#ifdef DEBUG
	output( Debug, "resolve: entering" );
#endif
	int mark = expression->result.mark;
	if ( mark ) {
#ifdef DEBUG
		outputf( Debug, "resolve: found mark=%d", mark );
#endif
		listItem *sub_results = NULL;
		for ( int count=0; count<3; count++ ) {
			if ( mark & ( 1 << count ) )
				extract_sub( &sub_results, expression, count, context );
		}
		if ( mark & 8 ) {
			if (( sub_results )) {
				extract_sub( &sub_results, expression, 3, context );
			}
			else return 1;
		}
		freeListItem( &expression->result.list );
		expression->result.list = sub_results;
		return 1;
	}

	// here we haven't found the mark yet
	ExpressionSub *sub = expression->sub;
	for ( int count = 0; count<4; count++ ) {
		if ( !sub[ count ].result.resolve )
			continue;
		Expression *e = sub[ count ].e;
		if (( count == 3 ) || sub[ 1 ].result.none ) {
			e->result.list = expression->result.list;
			expression->result.list = NULL;
		}
		else {
			listItem *sub_results = NULL;
			// extract subs and transport results up to the next level
			extract_sub( &sub_results, expression, count, context );
			freeListItem( &expression->result.list );
			e->result.list = sub_results;
		}

		if ( resolve( e, context ) ) {
			// transport the results all the way back to base
			expression->result.list = e->result.list;
			e->result.list = NULL;
			return 1;
		}
	}
	return 0; // should never happen
}

static void
extract_sub( listItem **list, Expression *expression, int count, _context *context )
{
	listItem *results = expression->result.list;
	if ( count == 3 ) {
		for ( listItem *i = results; i!=NULL; i=i->next )
			addIfNotThere( list, i->ptr );
	}
	else if ( context->expression.mode == ReadMode ) {
		for ( listItem *i = results; i!=NULL; i=i->next ) {
			Literal *l = l_sub( i->ptr, count );
			if (( l )) addIfNotThere( list, l );
		}
	}
	else	// not in ReadMode
	{
		for ( listItem *i = results; i!=NULL; i=i->next ) {
			Entity *e = ((Entity *) i->ptr )->sub[ count ];
			if (( e )) addIfNotThere( list, e );
		}
	}
}

/*---------------------------------------------------------------------------
	solve utilities
---------------------------------------------------------------------------*/
int
set_as_sub2( Expression *expression, int as_sub )
/*
	as_sub specifies that the expression results must be expression terms
	at as_sub position (e.g. if as_sub is 0, then expression results must
	be source in at least one other expression(s)).
	expression->result.as_sub specifies the same thing, but as a bitwise
	OR combination (e.g. if expression->result.as_sub is 3, then expression
	results must be source OR medium in other expression(s)).
	if as_sub is not 3, then we want as_sub to overrule the alternatives
	allowed by expression->result.as_sub.
*/
{
	int as_sub2 = expression->result.as_sub;
	if ( as_sub != 3 ) {
		as_sub2 &= 112;
		as_sub2 |= ( 1 << as_sub );
	}
	return as_sub2;
}

static int
test_as_sub( Entity *e, int as_sub2 )
{
	if ( as_sub2 & 7 )
	{
		if ((( as_sub2 & 1 ) && ( e->as_sub[ 0 ] != NULL )) ||
		    (( as_sub2 & 2 ) && ( e->as_sub[ 1 ] != NULL )) ||
		    (( as_sub2 & 4 ) && ( e->as_sub[ 2 ] != NULL )))
		{
			return 1;
		}
		return 0;
	}
	if ( as_sub2 &  112 )
	{
		if ((( as_sub2 & 16 ) && ( e->as_sub[ 0 ] != NULL )) ||
		    (( as_sub2 & 32 ) && ( e->as_sub[ 1 ] != NULL )) ||
		    (( as_sub2 & 64 ) && ( e->as_sub[ 2 ] != NULL )))
		{
			return 0;
		}
	}
	return 1;
}

int
set_active2( Expression *expression, int *not )
/*
	when an expression term has been set to any, the term has no associated
	values, but still we must screen candidates through possibly associated
	active and passive flags.

	set_active2() returns a bitwise combination for test_active() to perform
	the test as per the following logics:

	if ( sub[ 3 ].result.any ) {
		if ( sub[ 3 ].result.active ) {
			if ( not[ 3 ] ? cn_is_active( candidate ) : !cn_is_active( candidate ) ))
				return 0;
		}
		else if ( sub[ 3 ].result.passive ) {
			if ( not[ 3 ] ? !cn_is_active( candidate ) : cn_is_active( candidate ) ))
				return 0;
		}
	}
	for ( int count=0; count<3; count++ ) 
	{
		if ( !sub[ count ].result.any ) continue;
		if ( sub[ count ].result.active ) {
			Entity *e = candidate->sub[ count ];
			if (( e == NULL ) || ( not[ count ] ? cn_is_active( e ) : !cn_is_active( e )))
				return 0;
		}
		else if ( sub[ count ].result.passive ) {
			Entity *e = candidate->sub[ count ];
			if (( e == NULL ) || ( not[ count ] ? !cn_is_active( e ) : cn_is_active( e )))
				return 0;
		}
	}
*/
{
	int active = 0;
	ExpressionSub *sub = expression->sub;
	if ( sub[ 3 ].result.any ) {
		if ( sub[ 3 ].result.active )
			active |= not[ 3 ] ? 8 : 128;
		else if ( sub[ 3 ].result.passive )
			active |= not[ 3 ] ? 128 : 8;
	}
	for ( int count=0; count<3; count++ ) 
	{
		if ( !sub[ count ].result.any ) continue;
		if ( sub[ count ].result.active )
			active |= not[ count ] ? ( 1 << count ) : ( 16 << count );
		else if ( sub[ count ].result.passive )
			active |= not[ count ] ? ( 16 << count ) : ( 1 << count );
	}
	return active;
}

static int
test_active( Entity *e, int active2 )
{
	if ( active2 & 15 ) {
		if ((( active2 & 1 ) && (( e->sub[ 0 ] == NULL ) || !cn_is_active( e->sub[ 0 ] ))) ||
		    (( active2 & 2 ) && (( e->sub[ 1 ] == NULL ) || !cn_is_active( e->sub[ 1 ] ))) ||
		    (( active2 & 4 ) && (( e->sub[ 2 ] == NULL ) || !cn_is_active( e->sub[ 2 ] ))) ||
		    (( active2 & 8 ) && !cn_is_active( e )))
			return 0;
	}
	else if ( active2 & 240 ) {
		if ((( active2 & 16 ) && (( e->sub[ 0 ] == NULL ) || cn_is_active( e->sub[ 0 ] ))) ||
		    (( active2 & 32 ) && (( e->sub[ 1 ] == NULL ) || cn_is_active( e->sub[ 1 ] ))) ||
		    (( active2 & 64 ) && (( e->sub[ 2 ] == NULL ) || cn_is_active( e->sub[ 2 ] ))) ||
		    (( active2 & 128 ) && cn_is_active( e )))
			return 0;
	}
	return 1;
}

static listItem *
take_not( ExpressionSub *sub, int as_sub2, int active2, listItem *filter )
{
	if ( filter == NULL ) filter = CN.DB;
	listItem *list = sub->result.list;
	listItem *results = NULL;
	for ( listItem *i = filter; i!=NULL; i=i->next ) {
		Entity *e = i->ptr;
		if ( test_as_sub( e, as_sub2 ) && test_active( e, active2 ) && !(lookupItem( list, e )) )
			addItem( &results, e );
	}
	freeListItem( &sub->result.list );
	return results;
}

static listItem *
take_sub( ExpressionSub *sub, int as_sub2, int active2, listItem *filter )
{
	listItem *results = sub->result.list;
	sub->result.list = NULL;
	listItem *last_i = NULL, *next_i;
	if (( filter )) {
		for ( listItem *i = results; i!=NULL; i=next_i ) {
			next_i = i->next;
			Entity *e = i->ptr;
			if ( !test_as_sub( e, as_sub2 ) || !test_active( e, active2 ) || !(lookupItem( filter, e )) )
			{
				clipListItem( &results, i, last_i, next_i );
			}
			else last_i = i;
		}
	}
	else	// no filter
	{
		for ( listItem *i = results; i!=NULL; i=next_i ) {
			next_i = i->next;
			Entity *e = i->ptr;
			if ( !test_as_sub( e, as_sub2 ) || !test_active( e, active2 ) )
			{
				clipListItem( &results, i, last_i, next_i );
			}
			else last_i = i;
		}
	}
	return results;
}

static listItem *
take_all( Expression *expression, int as_sub2, int active2, listItem *filter )
{
	if ( filter == NULL ) filter = CN.DB;
	listItem *results = NULL;

	if ( expression->sub[ 1 ].result.none )
	{
		// optimization to limit the amount of results
		int count =
			( expression->result.mark == 1 ) ? 0 :
			( expression->result.mark == 2 ) ? 1 :
			( expression->result.mark == 4 ) ? 2 :
			3;
#ifdef DEBUG
		outputf( Debug, "taking all [.]: has_sub=%d, as_sub2=%d, active2=%d", count, as_sub2, active2 );
#endif
		for ( listItem *i = filter; i!=NULL; i=i->next )
		{
			Entity *e = i->ptr;
			if ( test_as_sub( e, as_sub2 ) && test_active( e, active2 ) &&
			     (( count == 3 ) || (e->sub[ count ]) ))
				addItem( &results, e );
		}
	}
	else {
#ifdef DEBUG
		outputf( Debug, "expression_solve: taking all [...]: as_sub2=%d, active2=%d", as_sub2, active2 );
#endif
		for ( listItem *i = filter; i!=NULL; i=i->next )
		{
			Entity *e = i->ptr;
			if ( (e->sub[ 0 ]) && (e->sub[ 1 ]) && (e->sub[ 2 ]) &&
			     test_as_sub( e, as_sub2 ) && test_active( e, active2 ))
				addItem( &results, e );
		}
	}
	return results;
}

static listItem *
take_sup( Expression *expression, int as_sup )
{
	listItem *results = NULL;
	for ( listItem *i = expression->result.list; i!=NULL; i=i->next ) {
		Entity *e = i->ptr;
		for ( int count=0; count<3; count++ ) {
			if ( !( as_sup & ( 1 << count ) ) )
				continue;
			for ( listItem *j = e->as_sub[ count ]; j!=NULL; j=j->next )
				addIfNotThere( &results, j->ptr );
		}
	}
	freeListItem( &expression->result.list );
	return results;
}

