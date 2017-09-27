#include <stdio.h>
#include <stdlib.h>

#include <stdarg.h>
#include <string.h>
#include <strings.h>

#include "database.h"
#include "registry.h"
#include "kernel.h"

#include "input.h"
#include "io.h"
#include "command.h"
#include "expression.h"
#include "narrative.h"
#include "variables.h"

// #define DEBUG

/*---------------------------------------------------------------------------
	is_frame_log_empty
---------------------------------------------------------------------------*/
int
is_frame_log_empty( _context *context )
{
	return (
		( context->frame.log.entities.instantiated == NULL ) &&
		( context->frame.log.entities.released == NULL ) &&
		( context->frame.log.entities.activated == NULL ) &&
		( context->frame.log.entities.deactivated == NULL ) &&
		( context->frame.log.narratives.activate == NULL ) &&
		( context->frame.log.narratives.deactivate == NULL )
	);
}

/*---------------------------------------------------------------------------
	reorder_log
---------------------------------------------------------------------------*/
int
reorder_log( _context *context )
{
	reorderListItem( &context->frame.log.entities.instantiated );
	reorderListItem( &context->frame.log.entities.released );
	reorderListItem( &context->frame.log.entities.activated );
	reorderListItem( &context->frame.log.entities.deactivated );
	return 0;
}

/*---------------------------------------------------------------------------
	register_results
---------------------------------------------------------------------------*/
/*
	register condition or event test results in occurrence->variables
*/
static void
register_results( Occurrence *occurrence, char *identifier, _context *context )
{
	if ( identifier == NULL ) return;

	VariableVA *variable;
	registryEntry *entry = lookupByName( occurrence->variables, identifier );
	if ( entry == NULL ) {
		variable = (VariableVA *) calloc( 1, sizeof(VariableVA) );
		registerByName( &occurrence->variables, strdup( identifier ), variable );
	}
	else {
		variable = (VariableVA *) entry->value;
		freeVariableValue( variable );
	}
	variable->type = ( context->expression.mode == ReadMode ) ? LiteralVariable : EntityVariable;
	variable->data.value = context->expression.results;
	context->expression.results = NULL;
}

/*---------------------------------------------------------------------------
	test_condition
---------------------------------------------------------------------------*/
static int
test_condition( Occurrence *occurrence, _context *context )
{
	for ( listItem *i = occurrence->va; i!=NULL; i=i->next )
	{
		ConditionVA *condition = (ConditionVA *) i->ptr;
		context->expression.mode = EvaluateMode;
		expression_solve( condition->expression, 3, context );
		if ( context->expression.results == NULL ) {
			return 0;
		}
		// register test results in occurrence->variables
		register_results( occurrence, condition->identifier, context );
	}
	return 1;
}

/*---------------------------------------------------------------------------
	search_and_register_events	- recursive
---------------------------------------------------------------------------*/
static int
test_condition_or_event( Narrative *instance, Occurrence *occurrence, Occurrence *thread, _context *context )
{
	switch ( occurrence->type ) {
	case ConditionOccurrence:
		if ( thread->type == EventOccurrence ) {
			return 0;	// condition following event => no need to search further events
		}
		return test_condition( occurrence, context );
	case EventOccurrence:
		break;
	default:
		return 0;
	}

	// test event
	for ( listItem *i = occurrence->va; i!=NULL; i=i->next )
	{
		EventVA *event = (EventVA *) i->ptr;
		if ( event->identifier.type == VariableIdentifier )
			return 0;	// DO_LATER

		if ( !event->type.notification )
			return 0;	// DO_LATER

		listItem *log = NULL;
		if ( event->type.instantiate )
			log = context->frame.log.entities.instantiated;
		else if ( event->type.activate )
			log = context->frame.log.entities.activated;
		else if ( event->type.deactivate )
			log = context->frame.log.entities.deactivated;
		else if ( event->type.release )
			log = context->frame.log.entities.released;
		if ( log == NULL )
			return 0;

		context->expression.mode = ( event->type.release ? ReadMode : EvaluateMode );
		context->expression.filter = log;
		int success = expression_solve( event->expression, 3, context );
		context->expression.filter = NULL;

		if ( success <= 0 ) return 0;	// No matching events logged in this frame

		// register test results in occurrence->variables
		register_results( occurrence, event->identifier.name, context );
	}
	return 1;
}

static void
search_and_register_events( Narrative *instance, Occurrence *thread, _context *context )
{
	int passed = 0;
	for ( listItem *i = thread->sub.n; i!=NULL; i=i->next )
	{
		Occurrence *occurrence = (Occurrence *) i->ptr;
		switch ( occurrence->type ) {
		case ConditionOccurrence:
			if (( passed = test_condition_or_event( instance, occurrence, thread, context ))) {
				search_and_register_events( instance, occurrence, context );
			}
			break;
		case EventOccurrence:
			passed = 1;
			if ( !occurrence->registered ) {
				if (( passed = test_condition_or_event( instance, occurrence, thread, context ))) {
					// register occurrence in context->frame.events
					addItem( &instance->frame.events, occurrence );
					occurrence->registered = 1;
				}
			}
			if ( passed ) {
				search_and_register_events( instance, occurrence, context );
			}
			break;
		case OtherwiseOccurrence:
			if ( passed )	// last pass's result
				break;
			passed = 1;	// this pass's result, so far
			if ( !occurrence->registered ) {
				for ( listItem *i=occurrence->va; i!=NULL; i=i->next ) {
					passed = test_condition_or_event( instance, i->ptr, thread, context );
					if ( !passed ) break;
				}
				if ( passed ) {
					// register occurrence in context->frame.events
					addItem( &instance->frame.events, occurrence );
					occurrence->registered = 1;
				}
			}
			if ( passed ) {
				search_and_register_events( instance, occurrence, context );
			}
			break;
		case ActionOccurrence:
			passed = 0;	// no 'otherwise' after do
			if ( thread->type == EventOccurrence ) {
				break;	// action following event
			}
			addItem( &instance->frame.actions, occurrence );
			break;

		case ThenOccurrence:
			passed = 0;	// no 'otherwise' after then
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
			if ( test_condition( occurrence, context ) ) {
				search_and_register_init( narrative, occurrence, context );
			}
			break;
		case EventOccurrence:
			// init cannot be vectorized - so no need to loop
			; EventVA *event = (EventVA *) occurrence->va->ptr;
			if ( event->type.init &&
			     ( event->identifier.name == NULL ) &&
			     ( event->expression == NULL ))
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
	for ( ;( event->type==EventOccurrence ) || ( event->type == OtherwiseOccurrence ); event=event->thread )
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
		if (( event->type == EventOccurrence ) || ( event->type == OtherwiseOccurrence ))
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
			if ( test_condition( occurrence, context ) ) {
				search_and_register_actions( narrative, occurrence, context );
			}
			break;
		case OtherwiseOccurrence:
		case EventOccurrence:
			if ( occurrence->registered ) {
				occurrence->registered = 0; // no need to take the same route twice
				search_and_register_actions( narrative, occurrence, context );
			}
			break;
		case ActionOccurrence:
			addItem( &narrative->frame.actions, occurrence );
			break;
		case ThenOccurrence:
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
	context->narrative.current = instance;
	context->narrative.level = context->control.level + 1;

	// frame.occurrences, frame.events and frame.actions are all LIFO
	// we must reorder the actions to execute them in FIFO order
	reorderListItem( &instance->frame.actions );

	for ( listItem *i = instance->frame.actions; i!=NULL; i=i->next )
	{
		Occurrence *occurrence = (Occurrence *) i->ptr;
		context->narrative.action = occurrence;
		ActionVA *action = (ActionVA *) occurrence->va->ptr;
		listItem *instruction = action->instructions;
		if ( instruction->next == NULL )
		{
			context->narrative.mode.action.one = 1;
			push_input( NULL, instruction->ptr, InstructionOne, context );
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

		if ( instance->deactivate )
			break;

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
        pop( base, 0, NULL, context );
	return 0;
}

/*---------------------------------------------------------------------------
	systemFrame
---------------------------------------------------------------------------*/
int
systemFrame( char *state, int e, char **next_state, _context *context )
{
	reorder_log( context );	// make fifo

#ifdef DEBUG
	output( Debug, "entering systemFrame" );
#endif
	// Check narratives to be deactivated - and deactivate them
	for ( registryEntry *i = context->frame.log.narratives.deactivate; i!=NULL; i=i->next )
	{
		Narrative *narrative = (Narrative *) i->identifier;
		listItem *entities = (listItem *) i->value;
		for ( listItem *j = entities; j!=NULL; j=j->next )
		{
#ifdef DEBUG
			output( Debug, "systemFrame: invoking deactivateNarrative()" );
#endif
			deactivateNarrative( (Entity *) j->ptr, narrative );
		}
		freeListItem( (listItem **) &i->value );
	}
	freeRegistry( &context->frame.log.narratives.deactivate );

	// perform actions registered from last frame
	for ( listItem *i = context->narrative.registered; i!=NULL; i=i->next )
	{
		Narrative *narrative = (Narrative *) i->ptr;
		for ( registryEntry *j = narrative->instances; j!=NULL; j=j->next )
		{
			Entity *e = (Entity *) j->identifier;
			Narrative *n = (Narrative *) j->value;
#ifdef DEBUG
			output( Debug, "systemFrame: invoking execute_narrative_actions()" );
#endif
			execute_narrative_actions( n, context );
			// check if the narrative reached 'exit'
			if ( n->deactivate ) {
				deactivateNarrative( e, narrative );
			}
		}
	}

	// actions may generate new occurrences
	if ( !is_frame_log_empty( context ) ) {
		// the translation from occurrence to events is performed below.
		// however it will take two more frames for the events to be
		// 1. translated into actions, and 2. executed
		context->frame.backlog = 2;
	} else if ( context->frame.backlog ) {
		context->frame.backlog--;
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
				output( Debug, "systemFrame: invoking search_and_register_actions()" );
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
			output( Debug, "systemFrame: invoking search_and_register_events() %0x",
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

	io_save_changes( context );

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
			output( Debug, "systemFrame: invoking activateNarrative()" );
#endif
			Narrative *n = activateNarrative( e, narrative );
			search_and_register_init( n, &n->root, context );
		}
		freeListItem( (listItem **) &i->value );
	}
	freeRegistry( &context->frame.log.narratives.activate );

	return 0;
}

