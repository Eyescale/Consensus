#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "database.h"
#include "registry.h"
#include "kernel.h"

#include "api.h"
#include "expression.h"
#include "variables.h"
#include "output.h"

// #define DEBUG

/*---------------------------------------------------------------------------
	set_filter_variable
---------------------------------------------------------------------------*/
int
set_filter_variable( _context *context )
{
	int retval = 1;

	char *identifier = context->expression.filter_identifier;
	if ( identifier == NULL )
		return 0;

	if ( context->expression.filter != NULL )
		return log_error( context, 0, "expression filter is already in place" );

	registryEntry *entry = lookupVariable( context, identifier );
	if ( entry == NULL ) {
		char *msg; asprintf( &msg, "variable '%%%s' not found", identifier );
		retval = log_error( context, 0, msg ); free( msg );
		return retval;
	}

	VariableVA *variable = (VariableVA *) entry->value;
	switch ( variable->type ) {
	case EntityVariable:
		; char *msg;
		if ( context->expression.mode == InstantiateMode ) {
			asprintf( &msg, "'%%%s' is an entity variable - "
				"cannot be used as expression filter in instantiation mode", identifier );
			retval = log_error( context, 0, msg ); free( msg );
			return retval;
		}
		context->expression.mode = EvaluateMode;
		retval = 0;
		break;
	case ExpressionVariable:
		asprintf( &msg, "'%%%s' is an expression variable - "
			"cannot be used as expression filter", identifier );
		retval = log_error( context, 0, msg ); free( msg );
		return retval;
	case LiteralVariable:
		context->expression.mode = ReadMode;
		break;
	case NarrativeVariable:
		asprintf( &msg, "'%%%s' is a narrative variable - "
			"cannot be used as expression filter", identifier );
		retval = log_error( context, 0, msg ); free( msg );
		return retval;
	}

	context->expression.filter = variable->data.value;
	context->expression.filter_identifier = NULL;
	free( identifier );
	return retval;
}

/*---------------------------------------------------------------------------
	rebuild_filtered_results
---------------------------------------------------------------------------*/
static Expression *
s_build( ExpressionSub *s )
{
	Expression *expression = (Expression *) calloc( 1, sizeof(Expression));
	ExpressionSub *sub = expression->sub;
	sub[ 3 ].result.any = 1;
	sub[ 3 ].e = expression;
	if ( s->e == NULL ) {
		switch ( s->result.identifier.type ) {
		case NullIdentifier:
			sub[ 0 ].result.identifier.type = NullIdentifier;
			break;
		case DefaultIdentifier:
			sub[ 0 ].result.identifier.type = DefaultIdentifier;
			sub[ 0 ].result.identifier.value = s->result.identifier.value;
			break;
		default:
			break;
		}
		sub[ 1 ].result.none = 1;
		sub[ 2 ].result.none = 1;
	}
	else for ( int i=0; i<3; i++ ) {
		ExpressionSub *s_sub = &s->e->sub[ i ];
		if ( s_sub->e == NULL ) {
			switch ( s_sub->result.identifier.type ) {
			case NullIdentifier:
				sub[ i ].result.identifier.type = NullIdentifier;
				break;
			case DefaultIdentifier:
				sub[ i ].result.identifier.type = DefaultIdentifier;
				sub[ i ].result.identifier.value = s_sub->result.identifier.value;
				break;
			default:
				sub[ i ].result.none = 1;
				break;
			}
		} else {
			sub[ i ].e = s_build( s_sub );
		}
	}
	return expression;
}

static int
s_compare( Expression *e1, Expression *e2 )
{
	ExpressionSub *sub1 = e1->sub;
	ExpressionSub *sub2 = e2->sub;

	for ( int i=0; i<3; i++ ) {
		if ( sub1[ i ].result.none ? !sub2[ i ].result.none : sub2[ i ].result.none )
			return 1;
		else if ( sub1[ i ].result.none )
			continue;
		else if ( sub1[ i ].e == NULL ) {
			if ( sub2[ i ].e != NULL )
				return 1;
			else if (( sub1[ i ].result.identifier.type == NullIdentifier ) ?
				 ( sub2[ i ].result.identifier.type != NullIdentifier ) :
				 ( sub2[ i ].result.identifier.type == NullIdentifier ))
				return 1;
			else if ( sub1[ i ].result.identifier.type == NullIdentifier )
				continue;
			else if ( strcmp( sub1[ i ].result.identifier.value, sub2[ i ].result.identifier.value ))
				return 1;
		}
		else if ( sub2[ i ].e == NULL )
			return 1;
		else if ( s_compare( sub1[ i ].e, sub2[ i ].e ) )
			return 1;
	}
	return 0;
}

static void
s_free( Expression *expression )
{
	if ( expression == NULL ) return;
	for ( int i=0; i<3; i++ ) {
		s_free( expression->sub[ i ].e );
	}
	free( expression );
}

static void
s_dup( Expression *e )
{
	ExpressionSub *sub = e->sub;
	for ( int i=0; i<3; i++ ) {
		if ( sub[ i ].result.none )
			continue;
		else if ( sub[ i ].e == NULL ) {
			switch ( sub[ i ].result.identifier.type ) {
			case NullIdentifier:
				break;
			case DefaultIdentifier:
				; char *identifier = sub[ i ].result.identifier.value;
				sub[ i ].result.identifier.value = strdup( identifier );
				break;
			default:
				break;
			}
		}
		else s_dup( sub[ i ].e );
	}
}

int
rebuild_filtered_results( listItem **results, _context *context )
{
	// results are expression-subs of the original filter variable
	// expressions. We need to turn them into standalone literals

	listItem *last_i = NULL, *next_i;
	for ( listItem *i = *results; i!=NULL; i=next_i )
	{
		next_i = i->next;
		Expression *e = s_build( i->ptr );

		// check & remove doublons
		int clipped = 0;
		for ( listItem *j = *results; j!=i; j=j->next )
		{
			if ( s_compare( ((ExpressionSub *) j->ptr )->e, e ) )
				continue;

			clipListItem( results, i, last_i, next_i );
			clipped = 1;
			s_free( e );
			break;
		}
		if ( !clipped ) {
			i->ptr = &e->sub[ 3 ];
			last_i = i;
		}
	}

	// now we can duplicate the strings
	for ( listItem *i = *results; i!=NULL; i=i->next ) {
		s_dup( ((ExpressionSub *) i->ptr )->e );
	}
	return ( *results != NULL );
}

/*---------------------------------------------------------------------------
	free_filter_results
---------------------------------------------------------------------------*/
void
free_filter_results( Expression *expression, _context *context )
{
	for ( listItem *i = expression->result.list; i!=NULL; i=i->next )
	{
		Expression *expression = ((ExpressionSub *) i->ptr )->e;
		freeExpression( expression );
	}
}

/*---------------------------------------------------------------------------
	remove_filter
---------------------------------------------------------------------------*/
int
remove_filter( Expression *expression, int mode, int as_sub, _context *context )
{
	context->expression.filter = NULL;

	// the current context->expression.mode is ReadMode

	switch ( mode ) {
	case EvaluateMode:
	case InstantiateMode:
	case ReleaseMode:
	case ActivateMode:
	case DeactivateMode:
		context->expression.mode = mode;
		break;
	default:
		return 1;
	}

	// now we need to perform the operation on the filtered results

	listItem *results = expression->result.list;
	listItem *last_results = NULL;
	for ( listItem *i = results; i!=NULL; i=i->next )
	{
		Expression *expression = ((ExpressionSub *) i->ptr )->e;
		int success = expression_solve( expression, as_sub, context );
		freeExpression( expression );
		if ( success > 0 ) {
			last_results = catListItem( context->expression.results, last_results );
			context->expression.results = NULL;
		}
	}
	freeListItem( &results );
	expression->result.list = last_results;
	return ( last_results != NULL );
}

