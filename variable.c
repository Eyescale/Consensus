#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "database.h"
#include "registry.h"
#include "kernel.h"
#include "output.h"
#include "input.h"
#include "input_util.h"

#include "api.h"
#include "variable.h"
#include "variable_util.h"
#include "expression.h"
#include "narrative.h"

// #define DEBUG

/*---------------------------------------------------------------------------
	execution engine	- local
---------------------------------------------------------------------------*/
#define do_( a, s ) \
	event = execute( a, &state, event, s, context );

static int
execute( _action action, char **state, int event, char *next_state, _context *context )
{
	int retval;
	if ( action == push ) {
		event = push( *state, event, &next_state, context );
		*state = base;
	} else {
		event = action( *state, event, &next_state, context );
		if ( strcmp( next_state, same ) )
			*state = next_state;
	}
	return event;
}

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
	VariableVA *variable = fetchVariable( context, *identifier, 1 );
	if ( !variable->type ) {
		variable->type = type;
	} else {
		freeVariableValue( variable );
		if ( type ) variable->type = type;
		if ( *identifier != variator_symbol )
			free( *identifier );
	}
	if ( *identifier != variator_symbol )
		*identifier = NULL;
	if ( type == StringVariable )
		variable->value = value;
	else
		addItem( (listItem **) &variable->value, value );
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

		variable = (VariableVA *) entry->value;
		freeVariableValue( variable );
		free( variable );

		registryDeregister( registry, identifier );
		break;

	case AddExpressionResults:
		variable = fetchVariable( context, NULL, 1 );
		if ( !variable->type ) {
			variable->type = ( context->expression.mode == ReadMode ) ? LiteralVariable : EntityVariable;
			variable->value = context->expression.results;
		}
		else if (( variable->type == EntityVariable ) ? ( context->expression.mode == ReadMode ) :
			 ( variable->type == LiteralVariable ) ? ( context->expression.mode != ReadMode ) : 1 )
		{
			output( Warning, "variable type mismatch: assigning variable" );
			freeVariableValue( variable );
			variable->type = ( context->expression.mode == ReadMode ) ? LiteralVariable : EntityVariable;
			variable->value = context->expression.results;
		}
		else {
			// add results to the variable's value - NOTE: will reverse results order
			variable->value = catListItem( variable->value, context->expression.results );
		}
		context->expression.results = NULL;
		break;

	case SetExpressionResults:
		if ( context->expression.results == NULL )
			return assign_( Reset, state, event, next_state, context );
		else if ( context->assignment.mode == AssignAdd )
			return assign_( AddExpressionResults, state, event, next_state, context );

		variable = fetchVariable( context, NULL, 1 );
		if ( variable->type ) freeVariableValue( variable );

		variable->type = ( context->expression.mode == ReadMode ) ? LiteralVariable : EntityVariable;
		variable->value = context->expression.results;
		context->expression.results = NULL;
		break;

	case SetValueAccount:
		if ( context->assignment.mode == AssignAdd )
			output( Warning, "variable type mismatch: assigning variable" );
		if ( context->expression.mode == ReadMode )
			translateFromLiteral( &context->expression.results, 3, context );
		if ( context->expression.results == NULL )
			return assign_( Reset, state, event, next_state, context );
		if ( strcmp( context->identifier.id[ 2 ].ptr, "literal" ) )
			return output( Error, "currently only 'literal' values can be assigned to variables" );

		variable = fetchVariable( context, NULL, 1 );
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
		if ( context->assignment.mode == AssignAdd )
			output( Warning, "variable type mismatch: assigning variable" );
		if ( context->expression.mode == ReadMode )
			translateFromLiteral( &context->expression.results, 3, context );
		if ( context->identifier.id[ 1 ].ptr == NULL )
			return assign_( Reset, state, event, next_state, context );
		if ( context->expression.results == NULL )
			return assign_( Reset, state, event, next_state, context );

		variable = fetchVariable( context, NULL, 1 );
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
		if ( context->assignment.mode == AssignAdd )
			output( Warning, "variable type mismatch: assigning variable" );

		Expression *expression = context->expression.ptr;
		if ( expression == NULL )
			return assign_( Reset, state, event, next_state, context );
#ifdef DEBUG
		output( Debug, "assign_expression: %s", identifier );
#endif
		// detect variable's self-references
		// ---------------------------------
		int count, type;
		identifier = context->identifier.id[ 0 ].ptr;
		count = count_occurrences( expression, identifier, 0 );
		if ( count > 0 ) {
			variable = lookupVariable( context, identifier, 0 );
			if ( variable == NULL )
				return output( Error, "self-referencing variable not found" );
			else if ( variable->type != ExpressionVariable )
				return output( Error, "self-referencing variable type mismatch" );
		}

		variable = fetchVariable( context, NULL, 1 );
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
	context->assignment.mode = 0;
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
	{ return assign_( SetExpressionResults, state, event, next_state, context ); }
int
assign_va( char *state, int event, char **next_state, _context *context )
	{ return assign_( SetValueAccount, state, event, next_state, context ); }
int
assign_narrative( char *state, int event, char **next_state, _context *context )
	{ return assign_( SetNarrative, state, event, next_state, context ); }
int
assign_expression( char *state, int event, char **next_state, _context *context )
	{ return assign_( SetExpression, state, event, next_state, context ); }

/*---------------------------------------------------------------------------
	read_variable_id, read_variable_ref
---------------------------------------------------------------------------*/
int
read_variable_id( char *state, int event, char **next_state, _context *context )
{
	int ndx = context->identifier.current;
	char *np = NULL, *path = NULL;
	state = base;
	do {
		bgn_
		in_( base ) bgn_
			on_any	do_( read_0, "step" )
			end
		in_( "step" ) bgn_
			on_( -1 )	do_( nothing, "" )
			on_other
				if ((path)) {
					asprintf( &np, "%s.%s", path, context->identifier.id[ ndx ].ptr );
					free( path ); path = np; np = NULL;
				}
				else {
					path = context->identifier.id[ ndx ].ptr;
					context->identifier.id[ ndx ].ptr = NULL;
				}
				bgn_
				on_( '.' )	do_( nop, base )
				on_other	do_( nothing, "" )
				end
			end
		end
	}
	while ( strcmp( state, "" ) );

	free( context->identifier.id[ ndx ].ptr );
	context->identifier.id[ ndx ].ptr = path;

	return event;
}

int
read_variable_ref( char *state, int event, char **next_state, _context *context )
{
	context->identifier.current = 1;
	event = read_variable_id( state, event, next_state, context );
	context->identifier.current = 0;
	return event;
}

