#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "database.h"
#include "registry.h"
#include "kernel.h"
#include "output.h"

#include "api.h"
#include "variable.h"
#include "variable_util.h"
#include "expression.h"
#include "narrative.h"

// #define DEBUG

/*---------------------------------------------------------------------------
	set_this_variable
---------------------------------------------------------------------------*/
void
set_this_variable( Registry *variables, Entity *e )
{
	VariableVA *variable = newVariable( variables, this_symbol );
	variable->type = EntityVariable;
	variable->value = newItem(e);
}

/*---------------------------------------------------------------------------
	assign_variable
---------------------------------------------------------------------------*/
void
assign_variable( char **identifier, void *value, int type, _context *context )
{
#ifdef DEBUG
	outputf( Debug, "assigning value to variable '%s'", identifier );
#endif
	StackVA *stack = (StackVA *) context->control.stack->ptr;

	VariableVA *variable;
	Registry *registry = variable_registry( context );
	registryEntry *entry = registryLookup( registry, *identifier );
	if ( entry == NULL ) {
		variable = newVariable( registry, *identifier );
		variable->type = type;
	}
	else {
		variable = (VariableVA *) entry->value;
		freeVariableValue( variable );
		if ( type ) variable->type = type;
		if ( *identifier != variator_symbol )
			free( *identifier );
	}
	if ( *identifier != variator_symbol )
		*identifier = NULL;
	if ( type == StringVariable ) {
		variable->value = value;
	} else {
		addItem( (listItem **) &variable->value, value );
	}
}

/*---------------------------------------------------------------------------
	assign_
---------------------------------------------------------------------------*/
enum {
	ResetAll = 1,
	Reset,
	AddExpressionResults,
	SetExpressionResults,
	SetValueAccount,
	SetNarrative,
	SetExpression
};

static int
assign_( int op, char *state, int event, char **next_state, _context *context )
{
	if ( !context_check( 0, 0, ExecutionMode ) )
		return 0;

	Registry *registry;
	registryEntry *entry;
	VariableVA *variable;
	char *identifier;

	switch ( op ) {
	case ResetAll:
		registry = variable_registry( context );
		registryEntry *r, *r_next, *r_last = NULL;
		for ( r = registry->value; r!=NULL; r=r_next )
		{
			r_next = r->next;
			identifier = r->index.name;
			if ( identifier == this_symbol ) {
				r_last = r;
				continue;
			}
			if ( identifier != variator_symbol )
				free( identifier );

			variable = (VariableVA *) r->value;
			freeVariableValue( variable );
			free( variable );

			if ( r_last == NULL )
				registry->value = r_next;
			else
				r_last->next = r_next;

			freeRegistryEntry( IndexedByName, r );
		}
		break;

	case Reset:
		identifier = context->identifier.id[ 0 ].ptr;
		if ( !strcmp( identifier, this_symbol ) )
			break;

		registry = variable_registry( context );
		entry = registryLookup( registry, identifier );
		if ( entry == NULL ) break;

		if ( entry->index.name != variator_symbol )
			free( entry->index.name );

		variable = (VariableVA *) entry->value;
		freeVariableValue( variable );
		free( variable );

		registryDeregister( registry, identifier );
		break;

	case AddExpressionResults:
		if ( context->expression.results == NULL )
			return assign_( Reset, state, event, next_state, context );

		registry = variable_registry( context );
		identifier = context->identifier.id[ 0 ].ptr;
		entry = registryLookup( registry, identifier );
		if ( entry == NULL ) {
			variable = newVariable( registry, identifier );
			context->identifier.id[ 0 ].ptr = NULL;
			variable->type = ( context->expression.mode == ReadMode ) ? LiteralVariable : EntityVariable;
			variable->value = context->expression.results;
		}
		else {
			variable = entry->value;
			int type = variable->type;
			int  mode = context->expression.mode;
			if (( type == EntityVariable ) ? ( mode == ReadMode ) :
			    ( type == LiteralVariable ) ? ( mode != ReadMode ) : 1 )
			{
				context->assignment.mode = 0;
				context->expression.results = NULL;
				return output( Error, "assignment mode not supported" );
			}
			// add results to the variable's value - NOTE: will reverse results order
			listItem *dst = variable->value;
			for ( listItem *i = context->expression.results; i!=NULL; i=i->next ) {
				dst->next = i;
				variable->value = dst;
			}
		}
		context->expression.results = NULL;
		break;

	case SetExpressionResults:
		if ( context->expression.results == NULL )
			return assign_( Reset, state, event, next_state, context );

		VariableVA *variable = fetchVariable( context );
		if ( variable->type ) freeVariableValue( variable );

		variable->type = ( context->expression.mode == ReadMode ) ? LiteralVariable : EntityVariable;
		variable->value = context->expression.results;
		context->expression.results = NULL;
		break;

	case SetValueAccount:
		if ( context->expression.mode == ReadMode )
			translateFromLiteral( &context->expression.results, 3, context );
		if ( context->expression.results == NULL )
			return assign_( Reset, state, event, next_state, context );
		if ( strcmp( context->identifier.id[ 2 ].ptr, "literal" ) )
			return output( Error, "currently only 'literal' values can be assigned to variables" );

		variable = fetchVariable( context );
		if ( variable->type ) freeVariableValue( variable );

		for ( listItem *i = context->expression.results; i!=NULL; i=i->next )
		{
			Expression *expression = cn_expression( i->ptr );
			i->ptr = &expression->sub[ 3 ];
		}
		variable->type = LiteralVariable;
		variable->value = context->expression.results;
		context->expression.results = NULL;
		break;

	case SetNarrative:
		if ( context->expression.mode == ReadMode )
			translateFromLiteral( &context->expression.results, 3, context );
		if ( context->identifier.id[ 1 ].ptr == NULL )
			return assign_( Reset, state, event, next_state, context );
		if ( context->expression.results == NULL )
			return output( Error, "narrative definition missing target entity" );

		variable = fetchVariable( context );
		if ( variable->type ) freeVariableValue( variable );

		variable->type = NarrativeVariable;
		variable->value = newRegistry( IndexedByAddress );
		identifier = context->identifier.id[ 1 ].ptr;
		for ( listItem *i = context->expression.results; i!=NULL; i=i->next )
		{
			Entity *e = (Entity *) i->ptr;
			Narrative *n = lookupNarrative( e, identifier );
			if ( n != NULL )
				registryRegister( (Registry *) variable->value, e, n->name );
		}
		break;

	case SetExpression:
		; Expression *expression = context->expression.ptr;
		if ( expression == NULL )
			return assign_( Reset, state, event, next_state, context );
#ifdef DEBUG
		output( Debug, "assign_expression: %s", identifier );
#endif
		// filter expression and, if not filtered, detect variable self-references
		// -----------------------------------------------------------------------
		identifier = context->identifier.id[ 0 ].ptr;
		int count, type;
		count = count_occurrences( expression, identifier, 0 );
		if ( count > 0 ) {
			registryEntry *entry = lookupVariable( context, identifier );
			if ( entry == NULL ) return output( Error, "self-referenced variable not found" );
			else if ( (((VariableVA *) entry->value )->type != ExpressionVariable ))
				return output( Error, "self-referenced variable type mismatch" );
		}

		variable = fetchVariable( context );
		switch ( variable->type ) {
		case EntityVariable:
		case NarrativeVariable:
		case LiteralVariable:
		case StringVariable:
			// variable was in the current scope but of a different type
			// no substitution in that case - user knows what he's doing
			freeVariableValue( variable );
			break;
		case ExpressionVariable:
			if ( count ) {
				expression_substitute( expression, identifier, variable->value, count );
			} else {
				freeVariableValue( variable );
			}
			break;
		default:
			// variable is newly created
			if ( count ) {
				// variable was higher-up on the stack and of the same type
				expression_substitute( expression, identifier, NULL, count );
			}
		}
		variable->type = ExpressionVariable;
		variable->value = newItem( context->expression.ptr );
		context->expression.ptr = NULL;
		break;
	}
	return 0;
}

/*---------------------------------------------------------------------------
	public interface
---------------------------------------------------------------------------*/
int
variable_reset( char *state, int event, char **next_state, _context *context )
	{ return assign_( Reset, state, event, next_state, context ); }
int
reset_variables( char *state, int event, char **next_state, _context *context )
	{ return assign_( ResetAll, state, event, next_state, context ); }
int
assign_results( char *state, int event, char **next_state, _context *context )
	{ return ( context->assignment.mode == AssignAdd ) ?
		assign_( AddExpressionResults, state, event, next_state, context ) :
		assign_( SetExpressionResults, state, event, next_state, context ); }
int
assign_va( char *state, int event, char **next_state, _context *context )
	{ return assign_( SetValueAccount, state, event, next_state, context ); }
int
assign_narrative( char *state, int event, char **next_state, _context *context )
	{ return assign_( SetNarrative, state, event, next_state, context ); }
int
assign_expression( char *state, int event, char **next_state, _context *context )
	{ return assign_( SetExpression, state, event, next_state, context ); }

