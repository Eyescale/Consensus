#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "database.h"
#include "registry.h"
#include "kernel.h"
#include "output.h"

#include "api.h"
#include "input_util.h"
#include "narrative_util.h"
#include "variable.h"
#include "variable_util.h"
#include "value.h"

// #define DEBUG

/*---------------------------------------------------------------------------
	set_va
---------------------------------------------------------------------------*/
static int
set_va_( char *va_name, int event, _context *context )
{
#ifdef DEBUG
	output( Debug, "set_va:$$(%s) expression %s", va_name, get_identifier( context, 1 ) );
#endif
	void *value;

	if ( !strcmp( va_name, "narratives" ) )
	{
		// find the identified narrative in the list of registered narratives
		char *name = get_identifier( context, 1 );
		Narrative *narrative = lookupNarrative( CN.nil, name );
		if ( narrative == NULL ) {
			return outputf( Error, "narrative '%s' is not instantiated - cannot assign", name );
		}

		for ( listItem *i = context->expression.results; i!=NULL; i=i->next )
		{
			Entity *e = (Entity *) i->ptr;
			// reset each of these entities' narratives value account to that single narrative
			Registry *registry = newRegistry( IndexedByName );
			registryRegister( registry, narrative->name, narrative );
			cn_va_set( e, va_name, registry );
			// register each of the passed entities to the narrative's list of entities
			addToNarrative( narrative, e );
		}
	}
	else {
		void *value = take_identifier( context, 1 );
		if ( value == NULL ) {
			return outputf( Error, "account '%s' cannot be set to (null) value", va_name );
		}
		for ( listItem *i = (listItem *) context->expression.results; i!=NULL; i=i->next ) {
			Entity *e = (Entity *) i->ptr;
			cn_va_set( e, va_name, value );
		}
	}
	return 0;
}

int
set_va( char *state, int event, char **next_state, _context *context )
{
#ifdef DEBUG
	output( Debug, "in set_va" );
#endif
	if ( !command_mode( 0, 0, ExecutionMode ) )
		return 0;

	if ( context->expression.results == NULL )
		return output( Error, "cannot assign value to (null) results" );

	char *va_name = get_identifier( context, 2 );
	if ( registryLookup( CN.VB, va_name ) == NULL ) {
		return outputf( Error, "unknown value account name '%s'", va_name );
	}
	return set_va_( va_name, event, context );
}

int
set_va_from_narrative( char *state, int event, char **next_state, _context *context )
	{ return set_va( state, event, next_state, context ); }

/*---------------------------------------------------------------------------
	set_va_from_variable	- narratives only
---------------------------------------------------------------------------*/
int
set_va_from_variable( char *state, int event, char **next_state, _context *context )
{
#ifdef DEBUG
	output( Debug, "in set_va_from_variable" );
#endif
	if ( !command_mode( 0, 0, ExecutionMode ) )
		return 0;

	if ( context->expression.results == NULL )
		return output( Error, "cannot assign value to (null) results" );

	// check value account name
	char *va_name = get_identifier( context, 2 );
	if ( registryLookup( CN.VB, va_name ) == NULL ) {
		return outputf( Error, "unknown value account name '%s'", va_name );
	}
	if ( strcmp( va_name, "narratives" ) ) {
		return output( Error, "only narrative accounts can be set from variable" );
	}

	// check variable name & type
	char *name = get_identifier( context, 1 );
	VariableVA *variable = lookupVariable( context, name, 0 );
	if ( variable == NULL ) {
		return outputf( Error, "variable '%s' is not set", name );
	}
	if ( variable->type != NarrativeVariable ) {
		return outputf( Error, "variable '%s' is not a narrative variable", name );
	}

	Registry *narratives = variable->value;
	Registry *copy = newRegistry( IndexedByName );
	for ( registryEntry *r = narratives->value; r!=NULL; r=r->next )
	{
		Entity *e = r->index.address;
		char *n = r->value;
		Narrative *narrative = lookupNarrative( e, n );
		if (( narrative ))
			registryRegister( copy, narrative->name, narrative );
	}
	if ( copy->value == NULL ) {
		free( copy );
		return 0;
	}

	for ( listItem *i = context->expression.results; i!=NULL; i=i->next, copy=registryDuplicate(copy) )
	{
		Entity *e = (Entity *) i->ptr;
		cn_va_set( e, va_name, copy );
		for ( registryEntry *j = copy->value; j!=NULL; j=j->next ) {
			addToNarrative((Narrative *) j->value, e );
		}
	}
	return 0;
}

