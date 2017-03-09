#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "database.h"
#include "registry.h"
#include "kernel.h"
#include "string_util.h"

#include "api.h"
#include "narrative.h"
#include "variables.h"
#include "value.h"

// #define DEBUG

/*---------------------------------------------------------------------------
	set_expression_value
---------------------------------------------------------------------------*/
static int
set_va_( char *va_name, int event, _context *context )
{
#ifdef DEBUG
	fprintf( stderr, "debug> set_va:$$(%s) expression %s\n", va_name, context->identifier.id[ 1 ].ptr );
#endif
	void *value;

	if ( !strcmp( va_name, "narratives" ) )
	{
		// find the identified narrative in the list of registered narratives
		char *name = context->identifier.id[ 1 ].ptr;
		Narrative *narrative = lookupNarrative( CN.nil, name );
		if ( narrative == NULL ) {
			char *msg; asprintf( &msg, "narrative '%s' is not instantiated", name );
			event = log_error( context, event, msg ); free( msg );
			return event;
		}

		for ( listItem *i = context->expression.results; i!=NULL; i=i->next )
		{
			Entity *e = (Entity *) i->ptr;
			// reset each of these entities' narratives value account to that single narrative
			cn_va_set_value( va_name, e, newRegistryItem( narrative->name, narrative ) );
			// register each of the passed entities to the narrative's list of entities
			addToNarrative( narrative, e );
		}
	}
	else if ( context->identifier.id[ 1 ].type != StringIdentifier ) {
		return log_error( context, event, "expected argument in \"quotes\"" );
	}
	else {
		void *value = string_extract( context->identifier.id[ 1 ].ptr );
		if ( value == NULL ) {
			char *msg; asprintf( &msg, "account '%s' cannot be set to (null) value", va_name );
			event = log_error( context, event, msg ); free( msg );
			return event;
		}
		for ( listItem *i = (listItem *) context->expression.results; i!=NULL; i=i->next ) {
			Entity *e = (Entity *) i->ptr;
			cn_va_set_value( va_name, e, value );
		}
	}
	return 0;
}

int
set_va( char *state, int event, char **next_state, _context *context )
{
#ifdef DEBUG
	fprintf( stderr, "debug> in set_va\n" );
#endif
	if ( !context_check( 0, 0, ExecutionMode ) )
		return 0;

	if ( context->expression.results == NULL )
		return log_error( context, event, "cannot assign value to (null) results" );

	char *va_name = context->identifier.id[ 2 ].ptr;
	if ( lookupByName( CN.VB, va_name ) == NULL ) {
		char *msg; asprintf( &msg, "unknown value account name '%s'", va_name );
		event = log_error( context, event, msg ); free( msg );
		return event;
	}
	return set_va_( va_name, event, context );
}

/*---------------------------------------------------------------------------
	set_expression_value_from_variable
---------------------------------------------------------------------------*/
int
set_va_from_variable( char *state, int event, char **next_state, _context *context )
{
#ifdef DEBUG
	fprintf( stderr, "debug> in set_va_from_variable\n" );
#endif
	if ( !context_check( 0, 0, ExecutionMode ) )
		return 0;

	if ( context->expression.results == NULL )
		return log_error( context, event, "cannot assign value to (null) results" );

	// check value account name
	char *va_name = context->identifier.id[ 2 ].ptr;
	if ( lookupByName( CN.VB, va_name ) == NULL ) {
		char *msg; asprintf( &msg, "unknown value account name '%s'", va_name );
		event = log_error( context, event, msg ); free( msg );
		return event;
	}
	if ( strcmp( va_name, "narratives" ) ) {
		return log_error( context, event, "only narrative accounts can be set from variable" );
	}

	// check variable name & type
	char *name = context->identifier.id[ 1 ].ptr;
	registryEntry *entry = lookupVariable( context, name );
	if ( entry == NULL ) {
		char *msg; asprintf( &msg, "variable '%s' is not set", name );
		event = log_error( context, event, msg ); free( msg );
		return event;
	}

	VariableVA *variable = (VariableVA *) entry->value;
	if ( variable->type != NarrativeVariable ) {
		char *msg; asprintf( &msg, "variable '%s' is not a narrative variable", name );
		event = log_error( context, event, msg ); free( msg );
		return event;
	}

	// extract the list of valid narratives from the passed list
	registryEntry *value = NULL;
	for ( registryEntry *i = (Registry) variable->data.value; i!=NULL; i=i->next )
	{
		Narrative *narrative = lookupNarrative( (Entity *) i->identifier, (char *) i->value );
		if ( narrative != NULL ) {
			registerByName( &value, narrative->name, narrative );
		}
	}
	if ( value == NULL ) return 0;

	listItem *narratives = (listItem *) variable->data.value;
	for ( listItem *i = context->expression.results; i!=NULL; i=i->next )
	{
		Entity *e = (Entity *) i->ptr;
		// reset each of these entities' narratives value account to the list
		cn_va_set_value( va_name, e, value );

		// register each of the passed entities to the narrative's list of entities
		for ( registryEntry *j = value; j!=NULL; j=j->next ) {
			addToNarrative((Narrative *) j->value, e );
		}

		if ( i->next != NULL ) {
			value = copyRegistry( value );
		}
	}
	return 0;
}

