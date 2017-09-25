#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "database.h"
#include "registry.h"
#include "kernel.h"
#include "output.h"

#include "api.h"
#include "variables.h"
#include "expression.h"
#include "narrative.h"

// #define DEBUG

/*---------------------------------------------------------------------------
	variable utilities
---------------------------------------------------------------------------*/
void
set_this_variable( Registry *variables, Entity *e )
{
	VariableVA *this = (VariableVA *) calloc( 1, sizeof(VariableVA) );
	this->type = EntityVariable;
	this->data.value = newItem( e );
	registerByName( variables, this_symbol, this );
}

void
freeVariableValue( VariableVA *variable )
{
	switch ( variable->type ) {
	case NarrativeVariable:
		freeRegistry((Registry *) &variable->data.value );
		break;
	case ExpressionVariable:
		freeExpression( ((listItem *) variable->data.value )->ptr );
		freeItem( variable->data.value );
		variable->data.value = NULL;
		break;
	case EntityVariable:
		freeListItem( (listItem **) &variable->data.value );
		break;
	case LiteralVariable:
		freeLiteral( (listItem **) &variable->data.value );
		break;
	}
}

void
freeVariables( registryEntry **variables )
{
	return;
	registryEntry *next_e;
	for ( registryEntry *entry = *variables; entry!=NULL; entry=next_e )
	{
		next_e = entry->next;
		char *name = entry->identifier;
		if (( name != variator_symbol ) && ( name != this_symbol ))
			free( name );
		VariableVA *variable = (VariableVA *) entry->value;
		if ( !variable->data.ref )
			freeVariableValue( variable );
		free( variable );
		freeRegistryItem( entry );
	}
	*variables = NULL;
}

registryEntry *
lookupVariable( _context *context, char *identifier )
{
	registryEntry *entry = NULL;
	if ( context->narrative.current &&
	    ( context->narrative.mode.action.one || context->narrative.mode.action.block ))
	{
		Occurrence *i = context->narrative.action;
		for ( ; ( i!=NULL ) && ( entry == NULL ); i=i->thread ) {
			entry = lookupByName( i->variables, identifier ); 
		}
	}
	// if we did not find it locally then we can check the program stack
	if ( entry == NULL ) {
		listItem *s = context->control.stack;
		for ( ; ( s!=NULL ) && ( entry==NULL ); s=s->next ) {
			StackVA *stack = (StackVA *) s->ptr;
			entry = lookupByName( stack->variables, identifier );
		}
	}
	return entry;
}

/*---------------------------------------------------------------------------
	variable_fetch	- lookup only in current scope
---------------------------------------------------------------------------*/
Registry *
variable_registry( _context *context )
{
	if ( context->narrative.current &&
	    ( context->narrative.mode.action.one || context->narrative.mode.action.block ))
	{
		Occurrence *occurrence = context->narrative.action;
		return &occurrence->variables;
	} else {
		StackVA *stack = (StackVA *) context->control.stack->ptr;
		return &stack->variables;
	}
}

registryEntry *
variable_fetch( char *identifier, _context *context )
{
	Registry *registry = variable_registry( context );
	return lookupByName( *registry, identifier );
}

void
variable_deregister( char *identifier, _context *context )
{
	Registry *registry = variable_registry( context );
	deregisterByAddress( registry, identifier );
}

VariableVA *
fetch_or_create_variable( char *identifier, int *created, _context *context )
{
	Registry *registry = variable_registry( context );
	registryEntry *entry = lookupByName( *registry, identifier );
	if ( entry == NULL ) {
		VariableVA *variable = (VariableVA *) calloc( 1, sizeof(VariableVA) );
		entry = registerByName( registry, identifier, variable );
		*created = 1;
	}
	else *created = 0;
	return entry->value;
}

/*---------------------------------------------------------------------------
	variable_reset, reset_variables
---------------------------------------------------------------------------*/
int
variable_reset( char *state, int event, char **next_state, _context *context )
{
	if ( !context_check( 0, 0, ExecutionMode ) )
		return 0;

	// fetch identified variable in current scope
	// ------------------------------------------

	char *identifier = context->identifier.id[ 0 ].ptr;
	if ( !strcmp( identifier, this_symbol ) )
		return 0;

	registryEntry *entry = variable_fetch( identifier, context );
	if ( entry == NULL ) return 0;

	char *name = (char *) entry->identifier;

	if ( name != variator_symbol ) free( name );
	VariableVA *variable = (VariableVA *) entry->value;
	if ( !variable->data.ref )
		freeVariableValue( variable );
	free( variable );

	variable_deregister( name, context );
	return 0;
}

int
reset_variables( char *state, int event, char **next_state, _context *context )
{
	if ( !context_check( 0, 0, ExecutionMode ) )
		return 0;

	Registry *variables = variable_registry( context );

	registryEntry *r, *r_next, *r_last = NULL;
	for ( r = *variables; r!=NULL; r=r_next )
	{
		r_next = r->next;
		char *name = r->identifier;
		if ( !strcmp( name, this_symbol ) )
			continue;

		if ( name != variator_symbol ) free( name );
		VariableVA *variable = (VariableVA *) r->value;
		if ( !variable->data.ref )
			freeVariableValue( variable );
		free( variable );

		if ( r_last == NULL )
			*variables = r_next;
		else
			r_last->next = r_next;

		freeRegistryItem( r );
	}
	return 0;
}

/*---------------------------------------------------------------------------
	assign_results
---------------------------------------------------------------------------*/
int
assign_results( char *state, int event, char **next_state, _context *context )
{
	if ( !context_check( 0, 0, ExecutionMode ) )
		return 0;

#ifdef DEBUG
	output( Debug, "assign_results: %s : expression-results", context->identifier.id[ 0 ].ptr );
#endif
	if ( context->expression.results == NULL ) {
		return output( Error, "cannot set variable to (null) results");
	}

	// fetch or create identified variable in current scope
	// ----------------------------------------------------

	int created;
	VariableVA *variable = fetch_or_create_variable( context->identifier.id[ 0 ].ptr, &created, context );
	if ( created ) {
		context->identifier.id[ 0 ].ptr = NULL;
	}
	else switch ( context->assignment.mode ) {
		case AssignAdd:
			switch ( variable->type ) {
				case EntityVariable:
					if ( context->expression.mode == ReadMode )
						goto ERROR;
					break;
				case LiteralVariable:
					if ( context->expression.mode != ReadMode )
						goto ERROR;
					break;
				default:
					goto ERROR;
			}
			// add results to the variable's value - NOTE: will reverse results order
			listItem *dst = variable->data.value;
			for ( listItem *i = context->expression.results; i!=NULL; i=i->next ) {
				dst->next = i;
				variable->data.value = dst;
			}
			context->expression.results = NULL;
			return 0;
		default:
			freeVariableValue( variable );
			free( context->identifier.id[ 0 ].ptr );
			context->identifier.id[ 0 ].ptr = NULL;
	}

	variable->type = ( context->expression.mode == ReadMode ) ? LiteralVariable : EntityVariable;
	variable->data.value = context->expression.results;
	context->expression.results = NULL;

	return 0;
ERROR:
	context->assignment.mode = 0;
	context->expression.results = NULL;
	return output( Error, " " );
}

/*---------------------------------------------------------------------------
	assign_va
---------------------------------------------------------------------------*/
int
assign_va( char *state, int event, char **next_state, _context *context )
{
	if ( !context_check( 0, 0, ExecutionMode ) )
		return 0;
#ifdef DEBUG
	output( Debug, "assign_va: %s : %%[_].$( %s )", context->identifier.id[ 0 ].ptr, context->identifier.id[ 2 ].ptr );
#endif
	if ( context->expression.mode == ReadMode ) {
		translateFromLiteral( &context->expression.results, 3, context );
	}
	if ( context->expression.results == NULL ) {
		return output( Error, "va assignment missing target entity" );
	}
	if ( strcmp( context->identifier.id[ 2 ].ptr, "literal" ) ) {
		return output( Error, "currently only 'literal' values can be assigned to variables" );
	}

	// fetch or create identified variable in current scope
	// ----------------------------------------------------

	int created;
	VariableVA *variable = fetch_or_create_variable( context->identifier.id[ 0 ].ptr, &created, context );
	if ( !created ) {
		freeVariableValue( variable );
		free( context->identifier.id[ 0 ].ptr );
	}
	context->identifier.id[ 0 ].ptr = NULL;

	// convert expression results into filter values
	// ---------------------------------------------

	for ( listItem *i = context->expression.results; i!=NULL; i=i->next )
	{
		Expression *expression = cn_expression( i->ptr );
		i->ptr = &expression->sub[ 3 ];
	}
	variable->type = LiteralVariable;
	variable->data.value = context->expression.results;
	context->expression.results = NULL;

	return 0;
}

/*---------------------------------------------------------------------------
	assign_narrative
---------------------------------------------------------------------------*/
int
assign_narrative( char *state, int event, char **next_state, _context *context )
{
	if ( !context_check( 0, 0, ExecutionMode ) )
		return 0;

#ifdef DEBUG
	output( Debug, "assign_narrative: %s : %s()", context->identifier.id[ 0 ].ptr, context->identifier.id[ 1 ].ptr );
#endif
	if ( context->expression.mode == ReadMode ) {
		translateFromLiteral( &context->expression.results, 3, context );
	}

	if ( context->identifier.id[ 1 ].ptr == NULL ) {
		return output( Error, "cannot set narrative variable to (null) results" );
	}
	if ( context->expression.results == NULL ) {
		return output( Error, "narrative definition missing target entity" );
	}

	// fetch or create identified variable in current scope
	// ----------------------------------------------------

	int created;
	VariableVA *variable = fetch_or_create_variable( context->identifier.id[ 0 ].ptr, &created, context );
	if ( !created ) {
		freeVariableValue( variable );
		free( context->identifier.id[ 0 ].ptr );
	}
	context->identifier.id[ 0 ].ptr = NULL;

	variable->type = NarrativeVariable;
	char *name = context->identifier.id[ 1 ].ptr;
	for ( listItem *i = context->expression.results; i!=NULL; i=i->next )
	{
		Entity *e = (Entity *) i->ptr;
		Narrative *n = lookupNarrative( e, name );
		if ( n != NULL ) {
			registerByAddress(( Registry *) &variable->data.value, e, n->name );
		}
	}

	return 0;
}

/*---------------------------------------------------------------------------
	assign_expression
---------------------------------------------------------------------------*/
static int
expression_substitute( Expression *expression, char *identifier, listItem *value, int count )
{
	if (( expression == NULL ) || !count )
		return count;

	ExpressionSub *sub = expression->sub;
	for ( int i=0; i<4; i++ ) {
		switch ( sub[ i ].result.identifier.type ) {
		case VariableIdentifier:
			; char *name = sub[ i ].result.identifier.value;
			if (( name == NULL ) || strcmp( name, identifier ) )
				continue;
			if ( value == NULL ) {
				sub[ i ].result.lookup = 1;
				count--;
			}
			else {
				free( name );
				sub[ i ].result.identifier.value = NULL;
				sub[ i ].e = value->ptr;
				if ( count == 1 ) {
					sub[ i ].result.identifier.type = 0;
					count = 0;
				}
			}
			break;
		default:
			if ( sub[ i ].result.any ) break;
			count = expression_substitute( sub[ i ].e, identifier, value, count );
		}
		if ( !count ) break;
	}
	if ( !count && ( value != NULL ) ) {
		// only collapse if the original count is 1
		expression_collapse( expression );
	}
	return count;
}

static int
count_occurrences( Expression *expression, char *identifier, int count )
{
	if ( expression == NULL ) return count;
	ExpressionSub *sub = expression->sub;
	for ( int i=0; i<4; i++ )
	{
		switch ( sub[ i ].result.identifier.type ) {
		case VariableIdentifier:
			; char *name = sub[ i ].result.identifier.value;
			if (( name == NULL ) || strcmp( name, identifier ) )
				continue;
			count++;
			break;
		default:
			count = count_occurrences( sub[ i ].e, identifier, count );
		}
	}
	return count;
}

int
assign_expression( char *state, int event, char **next_state, _context *context )
{
	if ( !context_check( 0, 0, ExecutionMode ) )
		return 0;

	Expression *expression = context->expression.ptr;
	if ( expression == NULL ) {
		return output( Error, "cannot assign variable to (null) expression" );
	}

#ifdef DEBUG
	output( Debug, "assign_expression: %s", identifier );
#endif

	// filter expression and, if not filtered, detect variable self-references
	// -----------------------------------------------------------------------

	char *identifier = context->identifier.id[ 0 ].ptr;
	int count, type;

	count = count_occurrences( expression, identifier, 0 );
	if ( count > 0 ) {
		registryEntry *entry = lookupVariable( context, identifier );
		if ( entry == NULL ) return output( Error, "self-referenced variable not found" );
		else if ( (((VariableVA *) entry->value )->type != ExpressionVariable ))
			return output( Error, "self-referenced variable type mismatch" );
	}

	// fetch or create identified variable in current scope
	// ----------------------------------------------------

	int created;
	VariableVA *variable = fetch_or_create_variable( context->identifier.id[ 0 ].ptr, &created, context );
	if ( created ) {
		context->identifier.id[ 0 ].ptr = NULL;
	}

	// assign variable's type and value
	// --------------------------------

	switch ( variable->type ) {
	case EntityVariable:
	case NarrativeVariable:
	case LiteralVariable:
		// variable was in the current scope but of a different type
		// no substitution in that case - user knows what he's doing
		freeVariableValue( variable );
		free( identifier );
		context->identifier.id[ 0 ].ptr = NULL;
		break;
	case ExpressionVariable:
		if ( count ) {
			expression_substitute( expression, identifier, variable->data.value, count );
		} else {
			freeVariableValue( variable );
		}
		free( identifier );
		context->identifier.id[ 0 ].ptr = NULL;
		break;
	default:
		// variable is newly created
		if ( count ) {
			// variable was higher-up on the stack and of the same type
			expression_substitute( expression, identifier, NULL, count );
		}
	}
	variable->type = ExpressionVariable;
	variable->data.value = newItem( context->expression.ptr );
	context->expression.ptr = NULL;
	return 0;
}

/*---------------------------------------------------------------------------
	assign_variator_variable
---------------------------------------------------------------------------*/
void
assign_variator_variable( Entity *index, int type, _context *context )
{
#ifdef DEBUG
	output( Debug, "assigning index to variator variable" );
#endif
	StackVA *stack = (StackVA *) context->control.stack->ptr;
	registryEntry *entry;

	VariableVA *variable;
	entry = lookupByName( stack->variables, variator_symbol );
	if ( entry == NULL ) {
		variable = (VariableVA *) calloc( 1, sizeof(VariableVA) );
		variable->type = type;
		registerByName( &stack->variables, variator_symbol, variable );
	}
	else {
		variable = (VariableVA *) entry->value;
		freeVariableValue( variable );
		if ( type ) variable->type = type;
	}
	addItem( (listItem **) &variable->data.value, index );
}

