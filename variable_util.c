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
#include "variable.h"
#include "variable_util.h"
#include "expression_util.h"
#include "narrative_util.h"
#include "xl.h"

// #define DEBUG

/*---------------------------------------------------------------------------
	newVariable
---------------------------------------------------------------------------*/
VariableVA *
newVariable( Registry *registry, char *identifier )
{
	VariableVA *variable = calloc( 1, sizeof(VariableVA) );
	registryRegister( registry, identifier, variable );
	return variable;
}

/*---------------------------------------------------------------------------
	fetchVariable	- retrieve or create variable based on context
---------------------------------------------------------------------------*/
static Registry * variable_registry( _context *context );

VariableVA *
fetchVariable( _context *context, char *identifier, int do_create )
{
	Registry *registry = variable_registry( context );
	registryEntry *entry = registryLookup( registry, identifier );
	return do_create ?
		( (entry) ? entry->value : newVariable( registry, identifier ) ):
		( (entry) ? entry->value : NULL );
}

static Registry *
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
	lookupVariable
---------------------------------------------------------------------------*/
VariableVA *
lookupVariable( _context *context, char *identifier )
{
	VariableVA *variable = NULL;
#if 0
	outputf( Debug, "lookupVariable: identifier='%s'", identifier );
#endif
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
	// if we did not find it locally then we check the program stack
	if ( variable == NULL )
	{
		Narrative *backup = context->narrative.current;
		context->narrative.current = NULL;

		listItem *stack = context->command.stack;
		for ( listItem *s = stack; s!=NULL; s=s->next )
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
	resetVariable
---------------------------------------------------------------------------*/
void
resetVariable( char *identifier, _context *context )
{
	if ( identifier == NULL )
		identifier = get_identifier( context, 0 );

	if ( !strcmp( identifier, this_symbol ) )
		return;

	Registry *registry = variable_registry( context );
	registryEntry *entry = registryLookup( registry, identifier );
	if ( entry == NULL ) return;

	VariableVA *variable = (VariableVA *) entry->value;
	freeVariableValue( variable );
	free( variable );

	registryDeregister( registry, identifier );
}

/*---------------------------------------------------------------------------
	resetVariables
---------------------------------------------------------------------------*/
void
resetVariables( _context *context )
{
	Registry *registry = variable_registry( context );
	registryEntry *r, *r_next, *r_last = NULL;
	for ( r = registry->value; r!=NULL; r=r_next )
	{
		r_next = r->next;
		char *identifier = r->index.name;
		if ( identifier == this_symbol ) {
			r_last = r;
			continue;
		}
		if ( identifier != variator_symbol )
			free( identifier );

		VariableVA *variable = r->value;
		freeVariableValue( variable );
		free( variable );

		if ( r_last == NULL )
			registry->value = r_next;
		else
			r_last->next = r_next;

		freeRegistryEntry( IndexedByName, r );
	}
}

/*---------------------------------------------------------------------------
	transferVariables
---------------------------------------------------------------------------*/
void
transferVariables( Registry *dst, Registry *src, int override )
{
	if ( dst->value == NULL ) {
		dst->value = src->value;
		src->value = NULL;
	}
	else {
		for ( registryEntry *r = src->value; r!=NULL; r=r->next ) {
			char *identifier = r->index.name;
			registryEntry *entry = registryLookup( dst, identifier );
			if (( entry )) {
				free( identifier );
				if ( override ) {
					freeVariableValue( entry->value );
					entry->value = r->value;
				}
				else freeVariableValue( r->value );
			}
			else registryRegister( dst, identifier, r->value );
		}
		emptyRegistry( src );
	}
}

/*---------------------------------------------------------------------------
	freeVariables
---------------------------------------------------------------------------*/
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
	freeVariableValue
---------------------------------------------------------------------------*/
void
freeVariableValue( VariableVA *variable )
{
	if ( variable->value == NULL ) return;

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
		freeLiterals( (listItem **) &variable->value );
		break;
	}
}

/*---------------------------------------------------------------------------
	setVariableValue
---------------------------------------------------------------------------*/
void
setVariableValue( VariableVA *variable, void *value, ValueType type, _context *context )
{
	freeVariableValue( variable );
	switch ( type ) {
	case NarrativeIdentifiers:
		variable->type = NarrativeVariable;
		break;
	case ExpressionValue:
		value = newItem( value );
		// no break
	case ExpressionCollect:
		variable->type = ExpressionVariable;
		break;
	case LiteralResults:
		variable->type = LiteralVariable;
		break;
	case EntityResults:
		variable->type = EntityVariable;
		break;
	case StringResults:
		variable->type = StringVariable;
		break;
	default:
		// cf. command.c: replace variator value w/o changing type
		break;
	}
	variable->value = value;
}

/*---------------------------------------------------------------------------
	addVariableValue
---------------------------------------------------------------------------*/
void
addVariableValue( VariableVA *variable, void *value, ValueType type, _context *context )
/*
	written so as to facilitate possible type conversion in the future
*/
{
	switch ( type ) {
	case NarrativeIdentifiers:
		switch ( variable->type ) {
		case EntityVariable:
		case StringVariable:
		case LiteralVariable:
		case ExpressionVariable:
			output( Warning, "addVariableValue: variable type mismatch: re-assigning variable" );
			freeVariableValue( variable );
			variable->type = NarrativeVariable;
			// no break
		case NarrativeVariable:
			if (( variable->value )) {
				Registry *r1 = variable->value;
				Registry *r2 = value;
				for ( registryEntry *r = r2->value; r!=NULL; r=r->next )
					registryRegister( r1, r->index.name, r->value );
				freeRegistry( (Registry **) &value );
			}
			else variable->value = value;
			break;
		}
		break;
	case ExpressionValue:
		value = newItem( value );
		// no break
	case ExpressionCollect:
		switch ( variable->type ) {
		case EntityVariable:
		case StringVariable:
		case LiteralVariable:
		case NarrativeVariable:
			output( Warning, "addVariableValue: variable type mismatch: re-assigning variable" );
			freeVariableValue( variable );
			variable->type = ExpressionVariable;
			// no break
		case ExpressionVariable:
			variable->value = catListItem( value, variable->value );
			break;
		}
		break;
	case LiteralResults:
		switch ( variable->type ) {
		case EntityVariable:
		case StringVariable:
		case ExpressionVariable:
		case NarrativeVariable:
			output( Warning, "addVariableValue: variable type mismatch: re-assigning variable" );
			freeVariableValue( variable );
			variable->type = LiteralVariable;
			// no break
		case LiteralVariable:
			variable->value = catListItem( value, variable->value );
			break;
		}
		break;
	case EntityResults:
		switch ( variable->type ) {
		case StringVariable:
		case LiteralVariable:
		case ExpressionVariable:
		case NarrativeVariable:
			output( Warning, "addVariableValue: variable type mismatch: re-assigning variable" );
			freeVariableValue( variable );
			variable->type = EntityVariable;
			// no break
		case EntityVariable:
			variable->value = catListItem( value, variable->value );
			break;
		}
		break;
	case StringResults:
		switch ( variable->type ) {
		case EntityVariable:
		case LiteralVariable:
		case ExpressionVariable:
		case NarrativeVariable:
			output( Warning, "addVariableValue: variable type mismatch: re-assigning variable" );
			freeVariableValue( variable );
			variable->type = StringVariable;
			// no break
		case StringVariable:
			variable->value = catListItem( value, variable->value );
			break;
		}
		break;
	default:
		// cf. command.c: replace variator value w/o changing type
		outputf( Debug, "***** Error: in addVariableValue: value untyped" );
		return;
	}
}

/*---------------------------------------------------------------------------
	getVariableValue
---------------------------------------------------------------------------*/
void *
getVariableValue( VariableVA *variable, _context *context, int duplicate )
{
	if ( variable == NULL ) {
		return NULL;
	}
	else if ( duplicate ) {
		listItem *results = NULL;
		switch ( variable->type ) {
		case EntityVariable:
			for ( listItem *i = variable->value; i!=NULL; i=i->next )
				addItem( &results, i->ptr );
			break;
		case StringVariable:
			for ( listItem *i = variable->value; i!=NULL; i=i->next )
				addItem( &results, strdup( i->ptr ) );
			break;
		case LiteralVariable:
			for ( listItem *i = variable->value; i!=NULL; i=i->next )
				addItem( &results, ldup( i->ptr ) );
			break;
		case ExpressionVariable:
			for ( listItem *i = variable->value; i!=NULL; i=i->next ) {
				listItem *s = x2s( i->ptr, context, 0 );
				Expression *x = s2x( popListItem(&s), context );
				addItem( &results, x );
			}
			break;
		case NarrativeVariable:
			// DO_LATER
			outputf( Debug, "***** in getVariableValue: NarrativeVariable duplication not supported" );
			break;
		}
		return results;
	}
	else {
		// provision: in the future we want to transport at least
		// EntityVariable value as string - safer, but also slower
		return variable->value;
	}
}

