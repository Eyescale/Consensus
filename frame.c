#include <stdio.h>
#include <stdlib.h>

#include <stdarg.h>
#include <string.h>
#include <strings.h>

#include "database.h"
#include "registry.h"
#include "kernel.h"

#include "input.h"
#include "command.h"
#include "expression.h"
#include "narrative.h"
#include "variables.h"

// #define DEBUG

/*---------------------------------------------------------------------------
	search_and_register_events	- recursive
---------------------------------------------------------------------------*/
static void
test_and_register_event( Narrative *instance, listItem *log, Occurrence *occurrence, _context *context )
{
	if ( occurrence->registered || ( log == NULL ))
		return;	// either registered already or no such events logged in this frame

	context->expression.mode = ( occurrence->va.event.type.release ? ReadMode : EvaluateMode );
	context->expression.filter = log;
	int success = expression_solve( occurrence->va.event.expression, 3, context );
	context->expression.filter = NULL;

	if ( success <= 0 ) return;	// No matching events logged in this frame

	// register results with event variable in instance->variables
	char *identifier = occurrence->va.event.identifier.name;
	if ( identifier != NULL ) {
		VariableVA *variable;
		registryEntry *entry = lookupByName( instance->variables, identifier );
		if ( entry == NULL ) {
			variable = (VariableVA *) calloc( 1, sizeof(VariableVA) );
			registerByName( &instance->variables, identifier, variable );
		}
		else {
			variable = (VariableVA *) entry->value;
			freeVariableValue( variable );
		}
		variable->type = ( occurrence->va.event.type.release ? LiteralVariable : EntityVariable );
		variable->data.value = context->expression.results;
		context->expression.results = NULL;
	}

	// register this occurrence in context->frame.events
	occurrence->registered = 1;
	addItem( &instance->frame.events, occurrence );
}

static void
test_and_register_narrative_event( Narrative *instance, Registry log, Occurrence *occurrence, _context *context )
{
	if ( occurrence->registered || ( log == NULL ) )
		return;	// either registered already or no such events logged in this frame

	if ( occurrence->va.event.expression == NULL ) {
		freeListItem( &context->expression.results );
		context->expression.results = newItem( CN.nil );
	} else {
		context->expression.mode = EvaluateMode;
		expression_solve( occurrence->va.event.expression, 3, context );
	}

	for ( listItem *i = context->expression.results; i!=NULL; i=i->next )
	{
		Entity *result = (Entity *) i->ptr;
		for ( registryEntry *j = log; j!=NULL; j=j->next )
		{
			Narrative *n = (Narrative *) j->identifier;
			// j->value holds the list of entities for which an instance of that narrative has been activated
			for ( listItem *k = (listItem *) j->value; k!=NULL; k=k->next )
			{
				Entity *e = (Entity *) k->ptr;
				if ( e != result ) continue;
				// find the pseudo used by that entity for that narrative
				registryEntry *entry = lookupByAddress( n->entities, e );
				char *name = ( entry->value == NULL ) ? n->name : (char *) entry->value;
				if ( !strcmp( occurrence->va.event.narrative_identifier, name ) ) {
					occurrence->registered = 1;
					addItem( &instance->frame.events, occurrence );
					return;
				}
			}
		}
	}
}

static void
search_and_register_events( Narrative *instance, Occurrence *thread, _context *context )
{
	for ( listItem *i = thread->sub.n; i!=NULL; i=i->next )
	{
		Occurrence *occurrence = (Occurrence *) i->ptr;
		switch ( occurrence->type ) {
		case ConditionOccurrence:
			if ( thread->type == EventOccurrence ) {
				break;	// condition following event
			}
			context->expression.mode = EvaluateMode;
			expression_solve( occurrence->va.condition.expression, 3, context );
			if ( context->expression.results != NULL ) {
				search_and_register_events( instance, occurrence, context );
			}
			break;
		case EventOccurrence:
			if ( occurrence->va.event.identifier.type == VariableIdentifier )
				break;	// DO_LATER

			if ( !occurrence->va.event.type.notification )
				break;	// DO_LATER

			if ( occurrence->va.event.type.narrative )
			{
				Registry log = NULL;
				if ( occurrence->va.event.type.instantiate )
					log = context->frame.log.narratives.instantiated;
				else if ( occurrence->va.event.type.release )
					log = context->frame.log.narratives.released;
				else if ( occurrence->va.event.type.activate )
					log = context->frame.log.narratives.activated;
				if ( occurrence->va.event.type.deactivate )
					log = context->frame.log.narratives.deactivated;
				test_and_register_narrative_event( instance, log, occurrence, context );
			}
			else
			{
				listItem *log = NULL;
				if ( occurrence->va.event.type.instantiate )
					log = context->frame.log.entities.instantiated;
				else if ( occurrence->va.event.type.activate )
					log = context->frame.log.entities.activated;
				else if ( occurrence->va.event.type.deactivate )
					log = context->frame.log.entities.deactivated;
				else if ( occurrence->va.event.type.release )
					log = context->frame.log.entities.released;
				test_and_register_event( instance, log, occurrence, context );
			}

			if ( occurrence->registered ) {
				search_and_register_events( instance, occurrence, context );
			}
			break;

		case ActionOccurrence:
			if ( thread->type == EventOccurrence ) {
				break;	// action following event
			}
			addItem( &instance->frame.actions, occurrence );
			break;

		case ThenOccurrence:
			break;
		}
	}
}

static void
search_and_register_init( Narrative *narrative, Occurrence *thread, _context *context )
{
	for ( listItem *i = thread->sub.n; i!=NULL; i=i->next )
	{
		Occurrence *occurrence = (Occurrence *) i->ptr;
		switch ( occurrence->type ) {
		case ConditionOccurrence:
			if ( thread->type == EventOccurrence ) {
				break;	// condition following event
			}
			context->expression.mode = EvaluateMode;
			expression_solve( occurrence->va.condition.expression, 3, context );
			if ( context->expression.results != NULL ) {
				search_and_register_init( narrative, occurrence, context );
			}
			break;
		case EventOccurrence:
			if ( occurrence->va.event.type.init &&
			     ( occurrence->va.event.identifier.name == NULL ) &&
			     ( occurrence->va.event.expression == NULL ))
			{
				occurrence->registered = 1;
				addItem( &narrative->frame.events, occurrence );
			}
			break;

		default:
			break;
		}
	}
}

/*---------------------------------------------------------------------------
	test_upstream_events
---------------------------------------------------------------------------*/
static int
test_upstream_events( Occurrence *event )
{
	for ( ; event->type==EventOccurrence; event=event->thread )
		if ( !event->registered ) return 0;
	return 1;
}

static void
deregister_events( Occurrence *event )
{
	event->registered = 0;
	for ( listItem *i = event->sub.n; i!=NULL; i=i->next )
	{
		event = (Occurrence *) i->ptr;
		if ( event->type == EventOccurrence )
			deregister_events( event );
	}
}

static void
filter_narrative_events( Narrative *narrative, _context *context )
{
	listItem *last_i = NULL, *next_i;
	listItem **list = &narrative->frame.events;

	// Keep only those events which the upstream events allow
	for ( listItem *i = *list; i!=NULL; i=next_i )
	{
		Occurrence *event = (Occurrence *) i->ptr;
		next_i = i->next;
		if ( !test_upstream_events( event ) )
		{
			if ( event->registered ) {
				deregister_events( event );
			}
			clipListItem( list, i, last_i, next_i );
		}
		else last_i = i;
	}
}

/*---------------------------------------------------------------------------
	search_and_register_actions	- recursive
---------------------------------------------------------------------------*/
static void
search_and_register_actions( Narrative *narrative, Occurrence *thread, _context *context )
{
	for ( listItem *i = thread->sub.n; i!=NULL; i=i->next )
	{
		Occurrence *occurrence = (Occurrence *) i->ptr;
		switch ( occurrence->type ) {
		case ConditionOccurrence:
			context->expression.mode = EvaluateMode;
			expression_solve( occurrence->va.condition.expression, 3, context );
			if ( context->expression.results != NULL ) {
				search_and_register_actions( narrative, occurrence, context );
			}
			break;
		case EventOccurrence:
			if ( occurrence->registered ) {
				occurrence->registered = 0; // no need to take the same route twice
				search_and_register_actions( narrative, occurrence, context );
			}
			break;
		case ActionOccurrence:
			addItem( &narrative->frame.actions, occurrence );
			break;
		default:
			break;
		}
	}
}

/*---------------------------------------------------------------------------
	execute_narrative_actions
---------------------------------------------------------------------------*/
static int
execute_narrative_actions( Narrative *instance, _context *context )
{
        push( base, 0, NULL, context );
	StackVA *stack = (StackVA *) context->control.stack->ptr;
	stack->variables = instance->variables;
	context->narrative.current = instance;
	context->narrative.level = context->control.level + 1;

	// frame.occurrences, frame.events and frame.actions are all LIFO
	// we must reorder the actions to execute them in FIFO order
	reorderListItem( &instance->frame.actions );

	for ( listItem *i = instance->frame.actions; i!=NULL; i=i->next )
	{
		Occurrence *occurrence = (Occurrence *) i->ptr;
		listItem *instruction = occurrence->va.action.instructions;
		if ( instruction->next == NULL )
		{
			context->narrative.mode.action.one = 1;
			push_input( NULL, instruction->ptr, APIStringInput, context );
			int event = read_command( base, 0, &same, context );
			pop_input( base, 0, NULL, context );
			context->narrative.mode.action.one = 0;
		}
		else
		{
			push( base, 0, NULL, context );
			int level = context->control.level;
			context->narrative.mode.action.block = 1;

			push_input( NULL, instruction, InstructionBlock, context );
			int event = read_command( base, 0, &same, context );
			pop_input( base, 0, NULL, context );

			if ( context->control.level == level ) {
				// returned from '/' - did not pop yet (expecting then)
				pop( base, 0, NULL, context );
				for ( listItem *j = occurrence->sub.n; j!=NULL; j=j->next ) {
					// we know there can be only one there...
					addItem( &instance->frame.then, j->ptr );
				}
				break;
			}
			context->narrative.mode.action.block = 0;
		}

		if ( instance->deactivate ) {
			freeListItem( &instance->frame.then );
			freeListItem( &instance->frame.events );
			break;
		}

		for ( listItem *j = occurrence->thread->sub.n; j!=NULL; j=j->next )
		{
			Occurrence *occurrence = (Occurrence *) j->ptr;
			if (( occurrence->type == ThenOccurrence ) && !occurrence->registered ) {
				occurrence->registered = 1;
				addItem( &instance->frame.then, occurrence );
			}
		}
	}

	freeListItem( &instance->frame.actions );
	context->narrative.level = 0;
	context->narrative.current = NULL;
	stack->variables = NULL;
        pop( base, 0, NULL, context );
	return 0;
}

/*---------------------------------------------------------------------------
	frame
---------------------------------------------------------------------------*/
int
systemFrame( char *state, int e, char **next_state, _context *context )
{
#ifdef DEBUG
	fprintf( stderr, "debug> entering systemFrame\n" );
#endif
	// perform actions registered from last frame, and update current conditions
	for ( listItem *i = context->narrative.registered; i!=NULL; i=i->next )
	{
		Narrative *narrative = (Narrative *) i->ptr;
		for ( registryEntry *j = narrative->instances; j!=NULL; j=j->next )
		{
			Entity *e = (Entity *) j->identifier;
			Narrative *n = (Narrative *) j->value;
#ifdef DEBUG
			fprintf( stderr, "debug> systemFrame: invoking execute_narrative_actions()\n" );
#endif
			execute_narrative_actions( n, context );
			if ( n->deactivate ) {
				deregisterByValue( &narrative->instances, n );
				registerByAddress( &context->frame.log.narratives.deactivated, e, n->name );
				n->name = NULL;
				freeNarrative( n );
			}
		}
	}

	// translate events registered last frame into new actions, based on current conditions
	for ( listItem *i = context->narrative.registered; i!=NULL; i=i->next )
	{
		Narrative *narrative = (Narrative *) i->ptr;
		for ( registryEntry *j = narrative->instances; j!=NULL; j=j->next )
		{
			Narrative *n = (Narrative *) j->value;
			for ( listItem *i = n->frame.events; i!=NULL; i=i->next )
			{
				Occurrence *event = (Occurrence *) i->ptr;
#ifdef DEBUG
				fprintf( stderr, "debug> systemFrame: invoking search_and_register_actions()\n" );
#endif
				if ( event->registered ) {
					event->registered = 0;	// no need to take the same route twice
					search_and_register_actions( n, event, context );
				}
			}
			freeListItem( &n->frame.events );
		}
	}

	// Transform latest occurrences into active narrative events, based on current conditions
	for ( listItem *i = context->narrative.registered; i!=NULL; i=i->next )
	{
		Narrative *narrative = (Narrative *) i->ptr;
		for ( registryEntry *j = narrative->instances; j!=NULL; j=j->next )
		{
#ifdef DEBUG
			fprintf( stderr, "debug> systemFrame: invoking search_and_register_events() %0x\n",
				(int) context->frame.log.entities.instantiated );
#endif
			Narrative *n = (Narrative *) j->value;
			search_and_register_events( n, &n->root, context );
			for ( listItem *j = n->frame.then; j!=NULL; j=j->next ) {
				Occurrence *then = (Occurrence *) j->ptr;
				search_and_register_events( n, then, context );
				then->registered = 0;
			}
			freeListItem( &n->frame.then );
		}
	}
	freeRegistry( &context->frame.log.narratives.instantiated );
	freeRegistry( &context->frame.log.narratives.released );
	freeRegistry( &context->frame.log.narratives.activated );
	freeRegistry( &context->frame.log.narratives.deactivated );
	freeListItem( &context->frame.log.entities.instantiated );
	freeListItem( &context->frame.log.entities.released );
	freeListItem( &context->frame.log.entities.activated );
	freeListItem( &context->frame.log.entities.deactivated );

	// Keep only those events which the upstream events allow
	for ( listItem *i = context->narrative.registered; i!=NULL; i=i->next )
	{
		Narrative *narrative = (Narrative *) i->ptr;
		for ( registryEntry *j = narrative->instances; j!=NULL; j=j->next )
		{
			Narrative *n = (Narrative *) j->value;
			filter_narrative_events( n, context );
		}
	}

	// Check narratives to be activated - and activate them
	for ( registryEntry *i = context->frame.log.narratives.activate; i!=NULL; i=i->next )
	{
		Narrative *narrative = (Narrative *) i->identifier;
		listItem *entities = (listItem *) i->value;
		for ( listItem *j = entities; j!=NULL; j=j->next )
		{
			Entity *e = (Entity *) j->ptr;
#ifdef DEBUG
			fprintf( stderr, "debug> systemFrame: invoking activateNarrative()\n" );
#endif
			Narrative *n = activateNarrative( e, narrative );
			search_and_register_init( n, &n->root, context );
		}
		freeListItem( &entities );
	}
	freeRegistry( &context->frame.log.narratives.activate );

	return 0;
}

