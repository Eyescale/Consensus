#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "database.h"
#include "registry.h"
#include "kernel.h"

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
		freeExpression( variable->data.value );
		variable->data.value = NULL;
		break;
	case EntityVariable:
		freeListItem( (listItem **) &variable->data.value );
		break;
	}
}

void
freeVariables( registryEntry **variables )
{
	registryEntry *next_e;
	for ( registryEntry *entry = *variables; entry!=NULL; entry=next_e )
	{
		next_e = entry->next;
		char *name = entry->identifier;
		if (( name != variator_symbol ) && ( name != this_symbol ))
			free( name );
		VariableVA *va = (VariableVA *) entry->value;
		if ( !va->data.ref )
			freeVariableValue( va );
		free( va );
		freeRegistryItem( entry );
	}
	*variables = NULL;
}

registryEntry *
lookupVariable( _context *context, char *identifier )
{
	registryEntry *entry = NULL;
	for ( listItem *s = context->control.stack; ( s!=NULL ) && ( entry==NULL ); s=s->next )
	{
		StackVA *stack = (StackVA *) s->ptr;
		entry = lookupByName( stack->variables, identifier );
	}
	return entry;
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
	fprintf( stderr, "debug> : %s : expression-results\n", context->identifier.id[ 0 ].ptr );
#endif
	if ( context->identifier.id[ 0 ].type != DefaultIdentifier ) {
		return log_error( context, event, "variable names cannot be in \"quotes\"" );
	}

	StackVA *stack = (StackVA *) context->control.stack->ptr;
	Entity *entity;

	// fetch latest expression results
	// -------------------------------------

	if ( context->expression.results == NULL ) {
		return log_error( context, event, "cannot set expression to (null) results");
	}

	// fetch or create identified variable in current scope
	// ----------------------------------------------------

	VariableVA *variable;

	registryEntry *entry = lookupByName( stack->variables, context->identifier.id[ 0 ].ptr );
	if ( entry == NULL ) {
		variable = (VariableVA *) calloc( 1, sizeof(VariableVA) );
		registerByName( &stack->variables, context->identifier.id[ 0 ].ptr, variable );
		context->identifier.id[ 0 ].ptr = NULL;
	}
	else {
		variable = (VariableVA *) entry->value;
		freeVariableValue( variable );
		free( context->identifier.id[ 0 ].ptr );
		context->identifier.id[ 0 ].ptr = NULL;
	}

	variable->data.value = context->expression.results;
	variable->type = EntityVariable;
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
	fprintf( stderr, "debug> : %s : %s()\n", context->identifier.id[ 0 ].ptr, context->identifier.id[ 1 ].ptr );
#endif
	if ( context->identifier.id[ 0 ].type != DefaultIdentifier ) {
		return log_error( context, event, "variable names cannot be in \"quotes\"" );
	}

	if ( context->identifier.id[ 1 ].ptr == NULL ) {
		return log_error( context, event, "cannot set narrative variable to (null) results" );
	}
	if ( context->expression.results == NULL ) {
		return log_error( context, event, "narrative definition missing target entity" );
	}

	// fetch or create identified variable in current scope
	// ----------------------------------------------------

	VariableVA *variable;

	StackVA *stack = (StackVA *) context->control.stack->ptr;
	registryEntry *entry = lookupByName( stack->variables, context->identifier.id[ 0 ].ptr );
	if ( entry == NULL ) {
		variable = (VariableVA *) calloc( 1, sizeof(VariableVA) );
		registerByName( &stack->variables, context->identifier.id[ 0 ].ptr, variable );
		context->identifier.id[ 0 ].ptr = NULL;
	}
	else {
		variable = (VariableVA *) entry->value;
		freeVariableValue( variable );
		free( context->identifier.id[ 0 ].ptr );
		context->identifier.id[ 0 ].ptr = NULL;
	}

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
expression_replace( Expression *expression, char *identifier, Expression *value, int count )
{
	if (( expression == NULL ) || !count )
		return count;

	ExpressionSub *sub = expression->sub;
	for ( int i=0; i<4; i++ ) {
		switch ( sub[ i ].result.identifier.type ) {
		case VariableIdentifier:
			; char *name = sub[ i ].result.identifier.name;
			if (( name == NULL ) || strcmp( name, identifier ) )
				continue;
			free( name );
			sub[ i ].result.identifier.name = NULL;
			sub[ i ].e = value;
			if ( count == 1 ) {
				sub[ i ].result.identifier.type = DefaultIdentifier;
				count = 0;
			}
			break;
		default:
			count = expression_replace( expression->sub[ i ].e, identifier, value, count );
		}
		if ( !count ) break;
	}
	if ( !count ) {
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
			; char *name = sub[ i ].result.identifier.name;
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
		return log_error( context, event, "cannot assign variable to (null) expression" );
	}

#ifdef DEBUG
	fprintf( stderr, "debug> : %s : \"%s\"\n", context->identifier.id[ 0 ].ptr, context->string.ptr );
#endif
	if ( context->identifier.id[ 0 ].type != DefaultIdentifier ) {
		return log_error( context, event, "variable names cannot be in \"quotes\"" );
	}

	StackVA *stack = (StackVA *) context->control.stack->ptr;
	Entity *entity;

	// fetch or create identified variable in current scope
	// ----------------------------------------------------

	VariableVA *variable;

	registryEntry *entry = lookupByName( stack->variables, context->identifier.id[ 0 ].ptr );
	if ( entry == NULL ) {
		variable = (VariableVA *) calloc( 1, sizeof(VariableVA) );
		registerByName( &stack->variables, context->identifier.id[ 0 ].ptr, variable );
		context->identifier.id[ 0 ].ptr = NULL;
	}
	else {
		variable = (VariableVA *) entry->value;
		if ( variable->type == EntityVariable ) {
			freeVariableValue( variable );
			free( context->identifier.id[ 0 ].ptr );
			context->identifier.id[ 0 ].ptr = NULL;
		}
	}

	if ( variable->type == ExpressionVariable )
	{
		char *identifier = context->identifier.id[ 0 ].ptr;
		Expression *original = (Expression *) variable->data.value;
		int count = count_occurrences( expression, identifier, 0 );
		expression_replace( expression, identifier, original, count );
		variable->data.value = expression;
		free( context->identifier.id[ 0 ].ptr );
		context->identifier.id[ 0 ].ptr = NULL;
	}
	else  {
		variable->data.value = context->expression.ptr;
		variable->type = ExpressionVariable;
	}
	context->expression.ptr = NULL;
	return 0;
}

/*---------------------------------------------------------------------------
	assign_variator_variable
---------------------------------------------------------------------------*/
void
assign_variator_variable( Entity *index, _context *context )
{
#ifdef DEBUG
	fprintf( stderr, "debug> assigning index to variator variable\n" );
#endif
	StackVA *stack = (StackVA *) context->control.stack->ptr;
	registryEntry *entry;

	VariableVA *variable;
	entry = lookupByName( stack->variables, variator_symbol );
	if ( entry == NULL ) {
		variable = (VariableVA *) calloc( 1, sizeof(VariableVA) );
		variable->type = EntityVariable;
		registerByName( &stack->variables, variator_symbol, variable );
	}
	else {
		variable = (VariableVA *) entry->value;
		freeVariableValue( variable );
		variable->type = EntityVariable;
	}

	addItem( (listItem **) &variable->data.value, index );
}

