#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "database.h"
#include "registry.h"
#include "kernel.h"
#include "output.h"

#include "api.h"
#include "variable_util.h"
#include "expression.h"
#include "narrative.h"

// #define DEBUG

#ifdef SUBVAR
Variable *
variableLookup( Registry *registry, char *identifier, int event, int create )
{
	registryEntry *entry;
	push_input( "", identifier, InstructionOne, context );
	do {
		event = read_0( base, event, &same, context );
		identifier = context->identifier.id[ 0 ].ptr;
		entry = registryLookup( registry, identifier );
		if ( entry == NULL ) {
			if ( !create ) return NULL;
			variable = (VariableVA *) calloc( 1, sizeof(VariableVA) );
			registryRegister( registry, identifier, variable );
			RTYPE( &variable->sub ) = IndexedByName;
			registry = &variable->sub;
		} else {
			variable = (VariableVA *) entry->value;
			registry = &variable->sub;
		}
	}
	while ( event == '.' );
	pop_input( base, 0, NULL, context );
	return variable;
}
#endif

/*---------------------------------------------------------------------------
	newVariable
---------------------------------------------------------------------------*/
VariableVA *
newVariable( Registry *registry, char *identifier )
{
	VariableVA *variable = calloc( 1, sizeof(VariableVA) );
	RTYPE( &variable->sub ) = IndexedByName;
	registryRegister( registry, identifier, variable );
	return variable;
}

/*---------------------------------------------------------------------------
	freeVariableValue, freeVariables
---------------------------------------------------------------------------*/
void
freeVariableValue( VariableVA *variable )
{
	switch ( variable->type ) {
	case NarrativeVariable:
		freeRegistry((Registry **) &variable->value );
		break;
	case ExpressionVariable:
		freeExpression( ((listItem *) variable->value )->ptr );
		freeItem( variable->value );
		variable->value = NULL;
		break;
	case EntityVariable:
		freeListItem( (listItem **) &variable->value );
		break;
	case StringVariable:
		for ( listItem *i = variable->value; i!=NULL; i=i->next )
			free( i->ptr );
		freeListItem( (listItem **) &variable->value );
		break;
	case LiteralVariable:
		freeLiteral( (listItem **) &variable->value );
		break;
	}
}

void
freeVariables( Registry *variables )
{
	registryEntry *next_r;
	for ( registryEntry *r = variables->value; r!=NULL; r=next_r )
	{
		next_r = r->next;
		char *name = r->index.name;
		if (( name != variator_symbol ) && ( name != this_symbol ))
			free( name );
		VariableVA *variable = (VariableVA *) r->value;
		freeVariableValue( variable );
		free( variable );
		freeRegistryEntry( IndexedByName, r );
	}
	variables->value = NULL;
}

/*---------------------------------------------------------------------------
	variable_registry
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

/*---------------------------------------------------------------------------
	lookupVariable
---------------------------------------------------------------------------*/
registryEntry *
lookupVariable( _context *context, char *identifier )
{
	registryEntry *entry = NULL;
	if ( context->narrative.current &&
	    ( context->narrative.mode.action.one || context->narrative.mode.action.block ))
	{
		Occurrence *i = context->narrative.action;
		for ( ; ( i!=NULL ) && ( entry == NULL ); i=i->thread ) {
			entry = registryLookup( &i->variables, identifier ); 
		}
	}
	// if we did not find it locally then we can check the program stack
	if ( entry == NULL ) {
		listItem *s = context->control.stack;
		for ( ; ( s!=NULL ) && ( entry==NULL ); s=s->next ) {
			StackVA *stack = (StackVA *) s->ptr;
			entry = registryLookup( &stack->variables, identifier );
		}
	}
	return entry;
}

/*---------------------------------------------------------------------------
	fetchVariable	- fetch or create variable based on context
---------------------------------------------------------------------------*/
VariableVA *
fetchVariable( _context *context )
{
	VariableVA *variable;
	char *identifier = context->identifier.id[ 0 ].ptr;
	Registry *registry = variable_registry( context );
	registryEntry *entry = registryLookup( registry, identifier );
	if ( entry == NULL ) {
		variable = newVariable( registry, identifier );
		context->identifier.id[ 0 ].ptr = NULL;
	}
	else variable = entry->value;
	return variable;
}

/*---------------------------------------------------------------------------
	expression_substitute
---------------------------------------------------------------------------*/
int
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

/*---------------------------------------------------------------------------
	count_occurrences
---------------------------------------------------------------------------*/
int
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

