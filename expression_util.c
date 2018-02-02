#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "database.h"
#include "registry.h"
#include "kernel.h"
#include "output.h"

#include "api.h"
#include "input.h"
#include "input_util.h"
#include "output_util.h"
#include "expression.h"
#include "variable.h"
#include "variable_util.h"
#include "expression_util.h"

// #define DEBUG

/*---------------------------------------------------------------------------
	newExpression
---------------------------------------------------------------------------*/
Expression *
newExpression( _context *context )
{
	Expression *expression = calloc( 1, sizeof(Expression) );
	expression->sub[ 3 ].result.any = 1;
	for ( int i=0; i<4; i++ ) {
		expression->sub[ i ].result.none = 1; // none meaning: none specified
	}
	return expression;
}

/*---------------------------------------------------------------------------
	freeExpression
---------------------------------------------------------------------------*/
void
freeExpression( Expression *expression )
{
	if ( expression == NULL ) return;
	ExpressionSub *sub = expression->sub;
	for ( int i=0; i<4; i++ )
	{
		if ( sub[ i ].result.any || sub[ i ].result.none )
			continue;

		freeExpression( sub[ i ].e );

		switch ( sub[ i ].result.identifier.type ) {
		case VariableIdentifier:
			if (( sub[ i ].result.identifier.value == variator_symbol ) ||
			    ( sub[ i ].result.identifier.value == this_symbol ))
				break;
			free( sub[ i ].result.identifier.value );
			break;
		case LiteralResults:
			freeLiterals( (listItem **) &sub[ i ].result.identifier.value );
			break;
		case EntityResults:
			freeListItem( (listItem **) &sub[ i ].result.identifier.value );
			break;
		case ExpressionCollect:
			freeExpressions( (listItem **) &sub[ i ].result.identifier.value );
			break;
		case DefaultIdentifier:
			free( sub[ i ].result.identifier.value );
			break;
		default:
			break;
		}
	}
	freeListItem( &expression->result.list );
	free( expression );
}

void
freeExpressions( listItem **list )
{
	for ( listItem *i = *list; i!=NULL; i=i->next )
		freeExpression( i->ptr );
	freeListItem( list );
}

/*---------------------------------------------------------------------------
	flagged, just_any, just_blank
---------------------------------------------------------------------------*/
int
flagged( Expression *expression, int i )
{
	ExpressionSub *sub = expression->sub;
	return ( sub[ i ].result.active || sub[ i ].result.passive || sub[ i ].result.not );
}

int
just_any( Expression *expression, int i )
{
	return	expression->sub[ i ].result.any && !flagged( expression, i );
}
int
just_blank( Expression *expression )
{
	for ( int i=0; i< 4; i++ )
		if ( !just_any( expression, i ) )
			return 0;
	return 1;
}

/*---------------------------------------------------------------------------
	freeLiteral
---------------------------------------------------------------------------*/
void
freeLiteral( Literal *l )
{
	freeExpression( l->e );
}

/*---------------------------------------------------------------------------
	freeLiterals
---------------------------------------------------------------------------*/
void
freeLiterals( listItem **list )
{
	for ( listItem *i = *list; i!=NULL; i=i->next ) {
		freeLiteral( i->ptr );
	}
	freeListItem( list );
}

/*---------------------------------------------------------------------------
	trace_mark, mark_negated
---------------------------------------------------------------------------*/
int
trace_mark( Expression *expression )
{
	if ( expression == NULL ) return 0;
	if ( expression->result.mark )
		return 1;
	ExpressionSub *sub = expression->sub;
	for ( int i=0; i<4; i++ ) {
		if ( sub[ i ].result.any )
			continue;
		if ( trace_mark( sub[ i ].e ) ) {
			sub[ i ].result.resolve = 1;
			return 1;
		}
	}
	return 0;
}
int
mark_negated( Expression *expression, int negated )
{
	if ( expression == NULL ) return 0;
	if ( expression->result.mark )
		return negated;
	ExpressionSub *sub = expression->sub;
	for ( int i=0; i<4; i++ ) {
		if ( sub[ i ].result.any )
			continue;
		if ( !sub[ i ].result.resolve )
			continue;
		if ( mark_negated( sub[ i ].e, negated || sub[ i ].result.not ) )
			return 1;
	}
	return 0;
}

/*---------------------------------------------------------------------------
	expression_collapse
---------------------------------------------------------------------------*/
static int collapse( void *dst, int *flags, int count, Expression *e );

void
expression_collapse( Expression *expression )
{
	if ( expression == NULL ) return;

	ExpressionSub *sub = expression->sub;
	int flags[ 3 ];

#ifdef DEBUG
	outputf( Debug, "expression_collapse: entering %0x", (int) expression );
#endif
	// we want to replace the whole current expression with its sub[ 3 ].e
	if ( (sub[ 3 ].e) && !sub[ 3 ].result.any && !flagged( expression, 3 ) &&
	     just_any( expression, 0 ) && just_any( expression, 1 ) && just_any( expression, 2 ))
	{
#ifdef DEBUG
		output( Debug, "expression_collapse: collapsing whole..." );
#endif
		flags[ 0 ] = 0;
		flags[ 1 ] = 0;
		collapse( expression, flags, 4, sub[ 3 ].e );
	}

	// we want to replace any sub[ i ] with its own sub-expression - e.g. [ [ expression ] ]
	for ( int i=0; i<4; i++ )
	{
		if ( !(sub[ i ].e) || sub[ i ].result.any )
			continue;

		Expression *e = sub[ i ].e;
		ExpressionSub *e_sub = e->sub;
		if ( e_sub[ 1 ].result.none && e_sub[ 3 ].result.any && !e->result.as_sub && !e->result.mark )
		{
#ifdef DEBUG
			outputf( Debug, "expression_collapse: collapsing sub[ %d ]", i );
#endif
			flags[ 0 ] = sub[ i ].result.active || e_sub[ 0 ].result.active;
			flags[ 1 ] = sub[ i ].result.passive || e_sub[ 0 ].result.passive;
			flags[ 2 ] = sub[ i ].result.not ^ e_sub[ 0 ].result.not;
			if ( flags[ 0 ] && flags[ 1 ] )
			{
				; // in this case we don't do anything
			}
			else collapse( &sub[ i ], flags, 1, e );
		}
	}

	if ( sub[ 0 ].result.none ) {
		int mark = expression->result.mark;
		if ( mark == 8 ) {
			sub[ 0 ].result.any = 1;
			sub[ 0 ].result.none = 0;
		}
		else if ( mark & 7 ) {
#ifdef DEBUG
			output( Debug, "expression_collapse: completing according to mark" );
#endif
			for ( int i=0; i<3; i++ ) {
				sub[ i ].result.any = 1;
				sub[ i ].result.none = 0;
			}
		}
	}

	if ( !sub[ 1 ].result.none )
		return;

	// we want to replace at least the body of the current expression with its sub[ 0 ].e
	else if ( (sub[ 0 ].e) && !sub[ 0 ].result.not )
	{
		Expression *e = sub[ 0 ].e;
		flags[ 0 ] = sub[ 3 ].result.active || sub[ 0 ].result.active;
		flags[ 1 ] = sub[ 3 ].result.passive || sub[ 0 ].result.passive;
		int as_sub = expression->result.as_sub | e->result.as_sub;
		if ( flags[ 0 ] && flags[ 1 ] )
		{
			; // in this case we don't do anything
		}
		else if (( as_sub & 7 ) ? !( as_sub & 112 ) : 1 )
		{
			if ( sub[ 3 ].result.any )
			{
#ifdef DEBUG
				output( Debug, "expression_collapse: collapsing whole..." );
#endif
				// in this case we replace the whole current expression with its sub[ 0 ].e
				collapse( expression, flags, 4, e );
				expression->result.as_sub = as_sub;
			}
			else if ( e->sub[ 3 ].result.any )
			{
#ifdef DEBUG
				output( Debug, "expression_collapse: collapsing body..." );
#endif
				// in this case we replace only the body of the current expression with sub[ 0 ].e
				collapse( expression, flags, 3, e );
				expression->result.as_sub = as_sub;
			}
		}
	}
	// we want to replace the current expression with its sub[ 3 ].e
	else if ( sub[ 0 ].result.any && (sub[ 3 ].e) && !sub[ 3 ].result.any )
	{
		flags[ 0 ] = sub[ 0 ].result.active || sub[ 3 ].result.active;
		flags[ 1 ] = sub[ 0 ].result.passive || sub[ 3 ].result.passive;
		if ( flags[ 0 ] && flags[ 1 ] )
		{
			; // in this case we don't do anything
		}
		else if ( sub[ 3 ].result.not )
		{
			// in this case we swap sub [ 3 ] and sub[ 0 ] - to be collapsed later
#ifdef DEBUG
			output( Debug, "collapse: swapping sub[ 0 ] and sub[ 3 ]" );
#endif
			memcpy( &sub[ 0 ], &sub[ 3 ], sizeof( ExpressionSub ) );
			sub[ 0 ].result.active = flags[ 0 ];
			sub[ 0 ].result.passive = flags[ 1 ];
			memset( &sub[ 3 ], 0, sizeof( ExpressionSub ) );
			sub[ 3 ].result.any = 1;
			sub[ 3 ].result.none = 1; // none specified
		}
		else
		{
#ifdef DEBUG
			output( Debug, "expression_collapse: collapsing whole..." );
#endif
			// in this case we replace the whole current expression with its sub[ 3 ]
			collapse( expression, flags, 4, sub[ 3 ].e );
		}
	}
}

static int
collapse( void *dst, int *flags, int count, Expression *e )
{
	flags[ 0 ] = flags[ 0 ] || e->sub[ 3 ].result.active;
	flags[ 1 ] = flags[ 1 ] || e->sub[ 3 ].result.passive;
	if ( flags[ 0 ] && flags[ 1 ] )
	{
		return 0; // in this case we don't do anything
	}
	else if ( count == 1 ) {
		ExpressionSub *sub = (ExpressionSub *) dst;
#ifdef DEBUG
		outputf( Debug, "collapse: collapsing %d", count );
#endif
		memcpy( sub, e->sub, sizeof( ExpressionSub ) );
		free( e );
		sub->result.active = flags[ 0 ];
		sub->result.passive = flags[ 1 ];
		sub->result.not = flags[ 2 ];
	}
	else {
#ifdef DEBUG
		outputf( Debug, "collapse: collapsing %d", count );
#endif
		Expression *expression = (Expression *) dst;
		expression->result.mark |= e->result.mark;
		expression->result.twist = e->result.twist;
		ExpressionSub *sub = expression->sub;
		for ( int j=0; j<count; j++ ) {
			memcpy( &sub[ j ], &e->sub[ j ], sizeof( ExpressionSub ) );
		}
		free( e );
		sub[ 3 ].result.active = flags[ 0 ];
		sub[ 3 ].result.passive = flags[ 1 ];
	}
	return 1;
}
