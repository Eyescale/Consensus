#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "database.h"
#include "registry.h"
#include "kernel.h"
#include "input.h"
#include "input_util.h"
#include "output.h"

#include "api.h"
#include "variable_util.h"
#include "expression_util.h"
#include "narrative_util.h"

// #define DEBUG

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
	freeVariables( &variable->sub );

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
	if ( context->narrative.current && !context->narrative.mode.editing )
	{
		Occurrence *occurrence = context->narrative.occurrence;
		return &occurrence->variables;
	} else {
		StackVA *stack = (StackVA *) context->command.stack->ptr;
		return &stack->variables;
	}
}

/*---------------------------------------------------------------------------
	fetchVariable	- retrieve or create variable based on context
---------------------------------------------------------------------------*/
VariableVA *
fetchVariable( _context *context, char *identifier, int do_create )
{
	VariableVA *variable = NULL;
	Registry *registry;
	registryEntry *entry;
	char *path;

	if ( identifier == NULL ) {
		path = context->identifier.id[ 0 ].ptr;
		registry = variable_registry( context );
	}
	else if ( strncmp( identifier, variator_symbol, strlen(variator_symbol) ) )
	{
		path = identifier;
		registry = variable_registry( context );
	}
	else	// variator
	{
		registry = variable_registry( context );
		entry = registryLookup( registry, variator_symbol );
		if ( entry == NULL ) {
			if ( do_create ) {
				variable = newVariable( registry, variator_symbol );
			}
			else return NULL;
		}
		else variable = (VariableVA *) entry->value;

		identifier += strlen( variator_symbol );
		if ( *identifier++ != '.' )
			return variable;

		path = identifier;
		registry = &variable->sub;
	}

	int event;
	IdentifierVA buffer = { NULL, NULL };
	do {
		while (((event=*path++)) && ( event != '.' ))
			string_append( &buffer, event );

		string_finish( &buffer, 0 );
		if ( buffer.ptr == NULL )
			continue;

		entry = registryLookup( registry, buffer.ptr );
		variable = do_create ?
			( (entry) ? entry->value : newVariable( registry, buffer.ptr ) ):
			( (entry) ? entry->value : NULL );

		if ((entry)) free( buffer.ptr );
		else if ( do_create ) buffer.ptr = NULL;
		else { free( buffer.ptr ); event = 0; }

		if ( event == '.' ) {
			registry = &variable->sub;
		}
	}
	while ( event );

	return variable;
}

/*---------------------------------------------------------------------------
	lookupVariable
---------------------------------------------------------------------------*/
VariableVA *
lookupVariable( _context *context, char *identifier, int up )
{
	VariableVA *variable = NULL;
	if ( context->narrative.current && !context->narrative.mode.editing )
	{
		Occurrence *occurrence = context->narrative.occurrence;
		for ( Occurrence *i = occurrence; i!=NULL; i=i->thread )
		{
			context->narrative.occurrence = i;
			variable = fetchVariable( context, identifier, 0 );
			if (( variable )) break;
		}
		context->narrative.occurrence = occurrence;
	}
	// if we did not find it locally then we can check the program stack
	if ( variable == NULL )
	{
		Narrative *backup = context->narrative.current;
		context->narrative.current = NULL;

		listItem *stack = context->command.stack;
		for ( listItem *s = (up?stack->next:stack); s!=NULL; s=s->next )
		{
			context->command.stack = s;
			variable = fetchVariable( context, identifier, 0 );
			if (( variable )) break;
		}
		context->command.stack = stack;

		context->narrative.current = backup;
	}
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

