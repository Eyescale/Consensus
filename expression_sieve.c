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

/*---------------------------------------------------------------------------
	sieve
---------------------------------------------------------------------------*/
static int l_test_as_sub( Literal *, int as_sub2 );
static int l_test_active( Literal *, int active2 );

int
sieve( Expression *expression, int as_sub, listItem *filter, _context *context )
/*
	Assumptions: filter is not null, context->expression.mode is ReadMode
	sieve() is the stripped-down version of solve() based on these premisses
*/
{
#ifdef DEBUG
	outputf( Debug, "sieve: entering: filter=%0x", filter );
#if 0
	output_expression( ExpressionAll, expression );
	output( Text, "\n" );
#endif
#endif
	ExpressionSub *sub = expression->sub;
	listItem *sub_filter;
	int success;

	// initialize test flags
	// ---------------------
	int any[ 4 ], not[ 4 ];
	for ( int count=0; count<4; count++ ) {
		any[ count ] = sub[ count ].result.any;
		not[ count ] = sub[ count ].result.not;
	}

	// set sub_filter to filtered instance results
	// -------------------------------------------
	success = sub_sieve( expression, 3, as_sub, filter, context );
	if ( success > 0 ) {
		sub_filter = sub[ 3 ].result.list;
		sub[ 3 ].result.list = NULL;
	}
	else return success;

	// extract other expression terms and solve expression
	// ---------------------------------------------------
	if ( sub[ 1 ].result.none )
	/*
		 special case: expression has no medium nor target terms
	*/
	{
		// extract source from filter
		success = sub_sieve( expression, 0, 3, sub_filter, context );
		if ( success > 0 ) {
			expression->result.list = sub[ 0 ].result.list;
			sub[ 0 ].result.list = NULL;
		}
	}
	else {
		// extract expression terms from filter
		for ( int count=0; count<3; count++ ) {
			success = sub_sieve( expression, count, 3, sub_filter, context );
			if ( success <= 0 ) goto RETURN_RESULTS;
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
	success = ( expression->result.list != NULL );

RETURN_RESULTS:
	// cleanup sub-filter
	if (( expression->result.list != sub_filter ) && ( sub_filter != filter ))
		freeListItem( &sub_filter );
	// cleanup sub-results
	for ( int count=0; count<4; count++ )
		freeListItem( &sub[ count ].result.list );

	// take sup if required
	if (( success > 0 ) && expression->result.as_sup ) {
		int as_sup = expression->result.as_sup;
		expression->result.list = l_take_sup( expression, as_sup );
	}
	else return success;

	return ( expression->result.list != NULL );
}

static int l_test_as_sub( Literal *l, int as_sub2 );
listItem *
l_take_sup( Expression *expression, int as_sup )
{
	listItem *results = NULL;
	for ( listItem *i = expression->result.list; i!=NULL; i=i->next ) {
		Literal *l = i->ptr;
		// each literal can only be involved in one relationship
		// instance - that is: with its super. As we use the same
		// bitwise convention to test as_sup and as_sub, we can
		// use l_test_as_sub to test the position of the literal
		// in its super.
		if ( l_test_as_sub( l, as_sup ) )
			addIfNotThere( &results, l->super );
	}
	freeListItem( &expression->result.list );
	return results;
}

/*---------------------------------------------------------------------------
	sub_sieve
---------------------------------------------------------------------------*/
static int l_test( Literal *l, int as_sub2, int active, int passive );
static listItem *l_clip( listItem **list, int active, int passive );
static listItem *sub_sieve_expressions( listItem *, int active, int passive, listItem *filter, _context *context );
static void invert( listItem **results, listItem **filter );

int
sub_sieve( Expression *expression, int count, int as_sub, listItem *filter, _context *context )
{
#ifdef DEBUG
	outputf( Debug, "sub_sieve: entering: count=%d, as_sub=%d, filter=%0x", count, as_sub, filter );
#endif
	ExpressionSub *sub = expression->sub;
	listItem **sub_results = &sub[ count ].result.list;

	int active = sub[ count ].result.active;
	int passive = sub[ count ].result.passive;
	int not = sub[ count ].result.not;

	// extract sub-filter
	// ------------------
	listItem *sub_filter = NULL;
	if ( sub[ count ].result.any ) {
		if (( count == 3 ) || sub[ 1 ].result.none ) {
			int as_sub2 = set_as_sub2( expression, as_sub );
			if ( as_sub2 || not || active || passive ) {
				for ( listItem *i = filter; i!=NULL; i=i->next ) {
					if ( l_test( i->ptr, as_sub2, active, passive ) ) {
						if ( !not ) addItem( &sub_filter, i->ptr );
					}
					else if ( not ) addItem( &sub_filter, i->ptr );
				}
			}
			else sub_filter = filter;
		}
		else {
#if 0
			outputf( Debug, "sub_sieve: count=%d, filter=%0x", count, (int)filter );
#endif
			for ( listItem *i = filter; i!=NULL; i=i->next ) {
				Literal *l = l_sub( i->ptr, count );
				if ( l == NULL ) continue;
				if ( l_test( l, 0, active, passive ) ) {
					if ( !not ) addItem( &sub_filter, l );
				}
				else if ( not ) addItem( &sub_filter, l );
			}
#if 0
			outputf( Debug, "sub_sieve: count=%d, sub_filter=%0x", count, (int)sub_filter );
#endif
		}
	}
	else if (( count == 3 ) || sub[ 1 ].result.none ) {
		int as_sub2 = set_as_sub2( expression, as_sub );
		if ( as_sub2 ) {
			for ( listItem *i = filter; i!=NULL; i=i->next ) {
				if ( l_test_as_sub( i->ptr, as_sub2 ) )
					addItem( &sub_filter, i->ptr );
			}
		}
		else sub_filter = filter;
	}
	else {
		for ( listItem *i = filter; i!=NULL; i=i->next ) {
			Literal *l = l_sub( i->ptr, count );
			if (( l )) {
#if 0
				outputf( Debug, "sub_sieve: adding to sub_filter for count=%d", count );
#endif
				addItem( &sub_filter, l );
			}
		}
	}
	if ( sub[ count ].result.any ) {
		*sub_results = sub_filter;
		return ( *sub_results != NULL );
	}
	if ( sub_filter == NULL )
		return 0;

	// screen expression term values through sub-filter
	// ------------------------------------------------
	void *value;

	switch ( sub_type( expression, count, &value, context ) ) {
	case IdentifierValueType:
		if ( active ) {
			Entity *e = cn_entity( value );
			if (( e == NULL ) || !cn_is_active( e ))
				break;
		}
		else if ( passive ) {
			Entity *e = cn_entity( value );
			if (( e == NULL ) || cn_is_active( e ))
				break;
		}
		for ( listItem *i = sub_filter; i!=NULL; i=i->next ) {
			Literal *l = i->ptr;
			if (( l->e ) && ( l->e->sub[ 1 ].result.none )) {
				l = &l->e->sub[ 0 ];
				char *s = l->result.identifier.value;
				if ( (s) && !strcmp( s, value ) )
					addItem( sub_results, i->ptr );
			}
			else {
				char *s = l->result.identifier.value;
				if ( (s) && !strcmp( s, value ) )
					addItem( sub_results, i->ptr );
			}
		}
		break;
	case EntityValueType:
		if (( active && !cn_is_active( value ) ) || ( passive && cn_is_active( value ) ))
			break;
		Literal *l = e2l( value );
		for ( listItem *i = sub_filter; i!=NULL; i=i->next ) {
			if ( !lcmp( i->ptr, l ) )
				addItem( sub_results, i->ptr );
		}
		freeLiteral( l );
		break;
	case EntitiesValueType:
		for ( listItem *i = value; i!=NULL; i=i->next ) {
			if (( active && !cn_is_active( i->ptr ) ) || ( passive && cn_is_active( i->ptr ) ))
				continue;
			Literal *l = e2l( value );
			for ( listItem *j = sub_filter; j!=NULL; j=j->next ) {
				if ( !lcmp( j->ptr, l ) )
					addItem( sub_results, j->ptr );
			}
			freeLiteral( l );
		}
		break;
	case LiteralsValueType:
		for ( listItem *i = value; i!=NULL; i=i->next ) {
			if ( !l_test( i->ptr, 0, active, passive ) )
				continue;
			for ( listItem *j = sub_filter; j!=NULL; j=j->next ) {
				if ( !lcmp( i->ptr, j->ptr ) )
					addItem( sub_results, j->ptr );
			}
		}
		break;
	case StringsValueType:
		// in case of embedded expressions: ward against infinite recursion
		if ( lookupItem( context->expression.stack, value ) )
			return output( Error, "recursion in expression: on string variable" );
		listItem *expressions = S2X( value, NULL, context );
		if (( expressions )) {
			addItem( &context->expression.stack, value );
			*sub_results = sub_sieve_expressions( expressions, active, passive, sub_filter, context );
			freeExpressions( &expressions );
			popListItem( &context->expression.stack );
		}
		break;
	case ExpressionsValueType:
		*sub_results = sub_sieve_expressions( value, active, passive, sub_filter, context );
		break;
	case SubValueType:
		; Expression *sub_e = value;
		int success = sieve( sub_e, 3, sub_filter, context );
		if ( success > 0 )
		/*
			Assumption: sub_filter cannot be returned - cf. expression_collapse()
		*/
		{
			*sub_results = l_clip( &sub_e->result.list, active, passive );
			sub_e->result.list = NULL;
		}
		break;
	default: // ErrorValueType
		if ( sub_filter != filter ) freeListItem( &sub_filter );
		return 0;
	}

	// invert results if required
	// --------------------------
	if ( sub[ count ].result.not ) {
		invert( sub_results, &sub_filter );
	}

	// return results
	// --------------
	if ( sub_filter != filter ) freeListItem( &sub_filter );
	return ( *sub_results != NULL );
}

/*---------------------------------------------------------------------------
	sieve utilities
---------------------------------------------------------------------------*/
static int
l_test_as_sub( Literal *l, int as_sub2 )
{
	if ( !as_sub2 ) return 1;
	if ( l->super == NULL ) return 0;
	Expression *sup = ((ExpressionSub *) l->super )->e;
	if ( as_sub2 & 7 )
	{
		if ( sup == NULL ) return 0;
		if ((( as_sub2 & 1 ) && ( &sup->sub[ 0 ] == l )) ||
		    (( as_sub2 & 2 ) && ( &sup->sub[ 1 ] == l )) ||
		    (( as_sub2 & 4 ) && ( &sup->sub[ 2 ] == l )))
		{
			return 1;
		}
		return 0;
	}
	if ( as_sub2 &  112 )
	{
		if ( sup == NULL ) return 1;
		if ((( as_sub2 & 16 ) && ( &sup->sub[ 0 ] == l )) ||
		    (( as_sub2 & 32 ) && ( &sup->sub[ 1 ] == l )) ||
		    (( as_sub2 & 64 ) && ( &sup->sub[ 2 ] == l )))
		{
			return 0;
		}
	}
	return 1;
}

static int
l_test( Literal *l, int as_sub2, int active, int passive )
{
	if ( active ) {
		Entity *e = l2e( l );
		if (( e == NULL ) || !cn_is_active( e ))
			return 0;
	}
	else if ( passive ) {
		Entity *e = l2e( l );
		if (( e == NULL ) || cn_is_active( e ))
			return 0;
	}
	return l_test_as_sub( l, as_sub2 );
}
static listItem *
l_clip( listItem **list, int active, int passive )
{
	if ( !active && !passive )
		return *list;

	listItem *last_i = NULL, *next_i;
	for ( listItem *i = *list; i!=NULL; i=next_i ) {
		next_i = i->next;
		if ( !l_test( i->ptr, 0, active, passive ) ) {
			clipListItem( list, i, last_i, next_i );
		}
		else last_i = i;
	}
	return *list;
}
static listItem *
sub_sieve_expressions( listItem *expressions, int active, int passive, listItem *filter, _context *context )
{
	listItem *results = NULL;
	for ( listItem *i = expressions; i!=NULL; i=i->next ) {
		int success = expression_solve( i->ptr, 3, filter, context );
		if ( success <= 0 ) continue;
		l_clip( &context->expression.results, active, passive );
		for ( listItem *j = context->expression.results; j!=NULL; j=j->next ) {
			for ( listItem *k = filter; k!=NULL; k=k->next )
				if ( !lcmp( k->ptr, j->ptr ) )
					addIfNotThere( &results, k->ptr );
		}
		freeLiterals( &context->expression.results );
	}
	return results;
}
static void
invert( listItem **results, listItem **filter )
{
	if ( *results == NULL ) {
		*results = *filter;
		*filter = NULL;
	}
	else {
		listItem *inverted = NULL;
		for ( listItem *i = *filter; i!=NULL; i=i->next ) {
			if ( !(lookupItem( *results, i->ptr )) ) {
				addItem( &inverted, i->ptr );
			}
		}
		freeListItem( results );
		*results = inverted;
	}
}

