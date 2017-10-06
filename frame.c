#include <stdio.h>
#include <stdlib.h>

#include <stdarg.h>
#include <string.h>
#include <strings.h>

#include "database.h"
#include "registry.h"
#include "kernel.h"

#include "frame.h"
#include "api.h"
#include "input.h"
#include "output.h"
#include "io.h"
#include "command.h"
#include "expression.h"
#include "narrative.h"
#include "variables.h"

// #define DEBUG

/*---------------------------------------------------------------------------
	test_log
---------------------------------------------------------------------------*/
int
test_log( _context *context, LogType type )
{
	switch ( type ) {
	case OCCURRENCES:
		; FrameLog *log = &context->io.input.log;
		return (
			( log->streams.instantiated ) ||
			( log->entities.instantiated ) ||
			( log->entities.released ) ||
			( log->entities.activated ) ||
			( log->entities.deactivated ) ||
			( log->narratives.activate ) ||
			( log->narratives.deactivate )
		);
	case EVENTS:
		for ( listItem *i = context->narrative.registered; i!=NULL; i=i->next ) {
			Narrative *n = (Narrative *) i->ptr;
			if (( n->frame.events )) return 1;
		}
		return 0;
	case ACTIONS:
		for ( listItem *i = context->narrative.registered; i!=NULL; i=i->next ) {
			Narrative *n = (Narrative *) i->ptr;
			if (( n->frame.actions )) return 1;
		}
		return 0;
	case THEN:
		for ( listItem *i = context->narrative.registered; i!=NULL; i=i->next ) {
			Narrative *n = (Narrative *) i->ptr;
			if (( n->frame.then )) return 1;
		}
		return 0;
	}
}

/*---------------------------------------------------------------------------
	frame_start
---------------------------------------------------------------------------*/
static int
frame_start( _context *context )
{
	FrameLog *log = &context->frame.log;
	memcpy( log, &context->io.input.log, sizeof(FrameLog) );
	bzero( &context->io.input.log, sizeof(FrameLog) );

	reorderListItem( &log->entities.instantiated );
	reorderListItem( &log->entities.released );
	reorderListItem( &log->entities.activated );
	reorderListItem( &log->entities.deactivated );

	return 0;
}

static void
register_init( Narrative *narrative )
{
	narrative->initialized = 1;
	// trigger next frame
	narrative->root.registered = 1;
	addItem( &narrative->frame.events, &narrative->root );
}

/*---------------------------------------------------------------------------
	frame_finish
---------------------------------------------------------------------------*/
static int
frame_finish( _context *context )
{
	// update backlog counter for io_scan()
	context->frame.backlog =
		test_log( context, OCCURRENCES ) ? 3 :
		test_log( context, THEN ) ? 3 :
		test_log( context, EVENTS ) ? 2 :
		test_log( context, ACTIONS ) ? 1 :
		0 ;

	// pass frame log to io for external notification
	memcpy( &context->io.output.log, &context->frame.log, sizeof(FrameLog) );
	bzero( &context->frame.log, sizeof(FrameLog) );

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
		expression_solve( condition->format, 3, context );
		if ( context->expression.results == NULL ) {
			return 0;
		}
		// register test results in occurrence->variables
		register_results( occurrence, condition->identifier, context );
	}
	return 1;
}

/*---------------------------------------------------------------------------
	test_event
---------------------------------------------------------------------------*/
static int
test_event( Occurrence *occurrence, _context *context )
{
	for ( listItem *i = occurrence->va; i!=NULL; i=i->next )
	{
		EventVA *event = (EventVA *) i->ptr;
#ifndef DO_LATER
		if ( !event->type.notification )
			return 0;
#else
		// handle event->type.request
		// handle event->identifier.type == VariableIdentifier
#endif	// DO_LATER

		int success = 0;
		FrameLog *log = &context->frame.log;
		if ( event->type.stream ) {
			freeListItem( &context->expression.results );
			for ( listItem *i = log->streams.instantiated; i!=NULL; i=i->next )
			{
				Entity *stream = (Entity *) i->ptr;
#ifndef DO_LATER
				if ( !strcmp( event->format, "*" ) ) {
					success = 1;
					addItem( &context->expression.results, stream );
					continue;
				}
#endif
				Entity *file = cn_getf( "?.[%..]", "is", stream, "has", "File" );
				if ( file == NULL ) {
					continue;
				}
				char *name = cn_name( file );
				if ( name == NULL ) {
					continue;
				}
				if ( strcmp( name, event->format ) ) {
					continue;
				}
				success = 1;
				addItem( &context->expression.results, stream );
			}
		} else {
			listItem *loglist = NULL;
			if ( event->type.instantiate )
				loglist = log->entities.instantiated;
			else if ( event->type.activate )
				loglist = log->entities.activated;
			else if ( event->type.deactivate )
				loglist = log->entities.deactivated;
			else if ( event->type.release )
				loglist = log->entities.released;

			if ( loglist == NULL ) return 0;

			context->expression.mode = ( event->type.release ? ReadMode : EvaluateMode );
			context->expression.filter = loglist;
			success = expression_solve( event->format, 3, context );
			context->expression.filter = NULL;
		}
		if ( success <= 0 ) return 0;	// No occurrence matching this event format logged in this frame

		// register test results in occurrence->variables
		register_results( occurrence, event->identifier.name, context );
	}
	return 1;
}

/*---------------------------------------------------------------------------
	test_condition_or_event	- invoked by search_and_register_events
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
		return test_event( occurrence, context );
		break;
	default:
		return 0;
	}
}

/*---------------------------------------------------------------------------
	search_and_register_events	- recursive
---------------------------------------------------------------------------*/
static void
search_and_register_events( Narrative *instance, Occurrence *thread, _context *context )
{
	int passed = 0;
	for ( listItem *i = thread->sub.n; i!=NULL; i=i->next )
	{
		Occurrence *occurrence = (Occurrence *) i->ptr;
		switch ( occurrence->type ) {
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
		case ActionOccurrence:
			passed = 0;	// no 'otherwise' after do
			if ( thread->type == EventOccurrence ) {
				break;	// action following event
			}
			if ( instance->initialized ) {
				addItem( &instance->frame.actions, occurrence );
			}
			break;

		case ThenOccurrence:
			passed = 0;	// no 'otherwise' after then
			break;
		}
	}
}

static void
search_and_register_init( Narrative *narrative, _context *context )
{
	int found = 0;
	for ( listItem *i = narrative->root.sub.n; i!=NULL; i=i->next )
	{
		Occurrence *occurrence = (Occurrence *) i->ptr;
		switch ( occurrence->type ) {
		case EventOccurrence:
			; EventVA *event = (EventVA *) occurrence->va->ptr;
			if ( event->type.init &&
			     ( event->identifier.name == NULL ) &&
			     ( event->format == NULL ))
			{
				found = 1;
				occurrence->registered = 1;
				addItem( &narrative->frame.events, occurrence );
			}
			break;
		case ConditionOccurrence:
		case ActionOccurrence:
		case ThenOccurrence:
		case OtherwiseOccurrence:
			break;
		}
	}
	if ( !found ) {
		register_init( narrative );
	}
}

/*---------------------------------------------------------------------------
	test_upstream_events
---------------------------------------------------------------------------*/
static int
test_upstream_events( Occurrence *event )
{
	for ( ; ( event->type==EventOccurrence ) || ( event->type == OtherwiseOccurrence ); event=event->thread )
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
	for ( listItem *i = narrative->frame.events; i!=NULL; i=next_i )
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

	// we must reorder the actions to execute them in FIFO order
	reorderListItem( &instance->frame.actions );

	for ( listItem *i = instance->frame.actions; i!=NULL; i=i->next )
	{
		Occurrence *occurrence = (Occurrence *) i->ptr;
		context->narrative.action = occurrence;
		ActionVA *action = (ActionVA *) occurrence->va->ptr;
		listItem *instruction = action->instructions;
		if ( instruction->next == NULL ) {
			cn_dof( "", instruction->ptr );
		} else {
			push( base, 0, NULL, context );
			int level = context->control.level;

			context->narrative.mode.action.block = 1;
			push_input( "", instruction, InstructionBlock, context );
			int event = read_command( base, 0, &same, context );
			pop_input( base, 0, NULL, context );

			if ( context->control.level == level ) {
				// returned from '/' - did not pop yet (expecting then)
				pop( base, 0, NULL, context );
				for ( listItem *j = occurrence->sub.n; j!=NULL; j=j->next ) {
					occurrence->registered = 1;
					addItem( &instance->frame.then, j->ptr );
				}
				break;
			}
			context->narrative.mode.action.block = 0;
		}

		if ( instance->deactivate )
			break;

		// look for a ThenOccurrence sibling from parent thread
		for ( Occurrence *thread = occurrence->thread; thread!=NULL; thread=thread->thread )
		{
			int found = 0;
			for ( listItem *j = thread->sub.n; j!=NULL; j=j->next ) {
				Occurrence *sibling = (Occurrence *) j->ptr;
				if (( sibling->type == ThenOccurrence ) && !sibling->registered ) {
					sibling->registered = 1;
					addItem( &instance->frame.then, sibling );
					found = 1;
					break;
				}
			}
			if ( found ) break;
		}
	}
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
#ifdef DEBUG
	output( Debug, "entering systemFrame" );
#endif
	frame_start( context );
	FrameLog *log = &context->frame.log;

#ifdef DEBUG
	output( Debug, "frame: 1. deactivate narratives" );
#endif
	// Check entity narratives to be deactivated - and deactivate them
	for ( registryEntry *i = log->narratives.deactivate; i!=NULL; i=i->next )
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

#ifdef DEBUG
	output( Debug, "frame: 2. execute actions" );
#endif
	// perform actions registered from last frame - including possibly exit
	for ( listItem *i = context->narrative.registered; i!=NULL; i=i->next )
	{
		Narrative *narrative = (Narrative *) i->ptr;
		for ( registryEntry *j = narrative->instances, *next_j; j!=NULL; j=next_j )
		{
			Entity *e = (Entity *) j->identifier;
			Narrative *n = (Narrative *) j->value;
			next_j = j->next;
#ifdef DEBUG
			output( Debug, "systemFrame: invoking execute_narrative_actions()" );
#endif
			execute_narrative_actions( n, context );
			if ( !n->initialized && ( n->frame.then == NULL )) {
				register_init( n );
			}
			// check if the narrative reached 'exit'
			if ( n->deactivate ) {
				deactivateNarrative( e, narrative );
			}
			freeListItem( &n->frame.actions );
		}
	}

#ifdef DEBUG
	output( Debug, "frame: 3. translate events into actions" );
#endif
	// translate events registered last frame into new actions, based on current conditions
	for ( listItem *i = context->narrative.registered; i!=NULL; i=i->next )
	{
		Narrative *narrative = (Narrative *) i->ptr;
		for ( registryEntry *j = narrative->instances; j!=NULL; j=j->next )
		{
			Narrative *n = (Narrative *) j->value;
			// reorder the events to parse them in FIFO order
			reorderListItem( &n->frame.events );
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

			if ( !n->initialized && ( n->frame.actions == NULL ))
				register_init( n );
		}
	}

#ifdef DEBUG
	output( Debug, "frame: 4. translate occurrences into events" );
#endif
	// Transform latest occurrences into active narrative events, based on current conditions
	for ( listItem *i = context->narrative.registered; i!=NULL; i=i->next )
	{
		Narrative *narrative = (Narrative *) i->ptr;
		for ( registryEntry *j = narrative->instances; j!=NULL; j=j->next )
		{
			Narrative *n = (Narrative *) j->value;
			if ( !n->initialized ) continue;

			search_and_register_events( n, &n->root, context );
			reorderListItem( &n->frame.then );
			for ( listItem *j = n->frame.then; j!=NULL; j=j->next ) {
				Occurrence *then = (Occurrence *) j->ptr;
				search_and_register_events( n, then, context );
				then->registered = 0;
			}
			freeListItem( &n->frame.then );
		}
	}

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

#ifdef DEBUG
	output( Debug, "frame: 5. activate narratives" );
#endif
	// Check narratives to be activated - and activate them
	for ( registryEntry *i = log->narratives.activate; i!=NULL; i=i->next )
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
			search_and_register_init( n, context );
		}
		freeListItem( (listItem **) &i->value );
	}

#ifdef DEBUG
	output( Debug, "leaving systemFrame" );
#endif
	frame_finish( context );
	return 0;
}

