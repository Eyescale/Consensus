#include <stdio.h>
#include <stdlib.h>

#include <stdarg.h>
#include <string.h>
#include <strings.h>

#include "database.h"
#include "registry.h"
#include "kernel.h"

#include "frame.h"
#include "io.h"
#include "api.h"
#include "input.h"
#include "output.h"
#include "output_util.h"
#include "command.h"
#include "expression_util.h"
#include "narrative_util.h"
#include "variable.h"
#include "variable_util.h"

// #define DEBUG

/*---------------------------------------------------------------------------
	test_log
---------------------------------------------------------------------------*/
int
test_log( _context *context, LogType type )
{
	switch ( type ) {
	case OCCURRENCES:
		; EntityLog *log = context->frame.log.entities.backbuffer;
		return (
			( log->instantiated ) ||
			( log->released ) ||
			( log->activated ) ||
			( log->deactivated ) ||
			( context->frame.log.narratives.activate.value ) ||
			( context->frame.log.narratives.deactivate.value )
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
	register_results
---------------------------------------------------------------------------*/
/*
	register condition or event test results in occurrence->variables
*/
static void
register_results( Occurrence *occurrence, char *identifier, _context *context )
{
	if ( identifier == NULL ) return;
	VariableVA *variable = fetchVariable( context, identifier, 1 );
	if ( variable->type ) freeVariableValue( variable );
	variable->type = ( context->expression.mode == ReadMode ) ? LiteralVariable : EntityVariable;
	variable->value = context->expression.results;
	context->expression.results = NULL;
}

/*---------------------------------------------------------------------------
	test_condition
---------------------------------------------------------------------------*/
static int
test_condition( Occurrence *occurrence, _context *context )
{
	int retval = ( occurrence->contrary ? 0 : 1 );
	for ( listItem *i = occurrence->va; i!=NULL; i=i->next )
	{
		ConditionVA *condition = (ConditionVA *) i->ptr;
		context->expression.mode = EvaluateMode;
		int success = expression_solve( condition->format, 3, context );
		if ( success <= 0 ) {
			if ( occurrence->contrary )
				retval = 1;
			else return 0;
		}
		// register test results in occurrence->variables
		register_results( occurrence, condition->identifier, context );
	}
	return retval;
}

/*---------------------------------------------------------------------------
	test_event
---------------------------------------------------------------------------*/
static listItem *
lookupEntityLog( EventVA *event, EntityLog *log, _context *context )
{
	int success = 0;
	listItem *loglist;

	switch ( event->type ) {
	case InitEvent:
		return NULL; // registered only once
	default:
		loglist =
			( event->type == InstantiateEvent ) ? log->instantiated :
			( event->type == ReleaseEvent ) ? log->released :
			( event->type == ActivateEvent ) ? log->activated :
			( event->type == DeactivateEvent ) ? log->deactivated : NULL;

		if ( loglist == NULL ) return NULL;

		context->expression.mode = ( event->source.scheme || ( event->type == ReleaseEvent )) ?
			ReadMode : EvaluateMode;

		context->expression.filter = loglist;
		success = expression_solve( event->format, 3, context );
		context->expression.filter = NULL;
	}
	return ( success > 0 ) ? context->expression.results : NULL;
}

static int
test_event( Narrative *instance, EntityLog *log, Occurrence *occurrence, _context *context )
{
	for ( listItem *i = occurrence->va; i!=NULL; i=i->next )
	{
		EventVA *event = (EventVA *) i->ptr;
		switch ( event->source.scheme ) {
		case CGIScheme:
			log = &context->frame.log.cgi;
			break;
		case SessionScheme:
			// DO_LATER
			break;
		case OperatorScheme:
			// DO_LATER
			break;
		case FileScheme:
			// DO_LATER
			break;
		default:
			break;
		}
		if ( lookupEntityLog( event, log, context ) == NULL )
			return 0;

		// register test results into occurrence->variables
		register_results( occurrence, event->identifier, context );
	}
	return 1;
}

/*---------------------------------------------------------------------------
	search_and_register_events	- recursive
---------------------------------------------------------------------------*/
static int
search_and_register_events( Narrative *instance, EntityLog *log, Occurrence *thread, _context *context )
{
	OccurrenceType last;
	int passed = 1, todo = 0;
	for ( listItem *i = thread->sub.n; i!=NULL; i=i->next )
	{
		Occurrence *occurrence = (Occurrence *) i->ptr;
		context->narrative.occurrence = occurrence;

		switch ( occurrence->type ) {
		case OtherwiseOccurrence:
			if ( passed )	// last pass's result
				break;
			passed = 1;	// this pass's result, so far
			if ( !occurrence->registered ) {
				listItem *i = occurrence->va;
				// in case last was an EventOccurrence then this is an otherwise
				// _event_. in case this event comes with conditions, the conditions
				// will be evaluated later - during the next frame - but this
				// occurrence must be registered as an event.
				int on_else_in = 0;
				if ( i != NULL ) {
					Occurrence *o = i->ptr;
					if (( last == EventOccurrence ) && ( o->type == ConditionOccurrence ))
						{ on_else_in = 1; passed = 0; }
					last = o->type;
				}
				if ( !on_else_in ) for ( ; i!=NULL; i=i->next ) {
					Occurrence *o = i->ptr;
					passed = ( o->type == EventOccurrence ) ? test_event( instance, log, o, context ) :
						 ( o->type == ConditionOccurrence ) && ( thread->type != EventOccurrence ) ?
							 test_condition( o, context ) : 0;
					if ( !passed ) break;
				}
				if (( passed && ( last == EventOccurrence )) || on_else_in ) {
					// register occurrence in context->frame.events
					// conditions will have to be evaluated later
					addItem( &instance->frame.events, occurrence );
					occurrence->registered = 1;
				}
			}
			if ( passed ) {
				todo |= search_and_register_events( instance, log, occurrence, context );
			}
			break;
		case ConditionOccurrence:
			last = ConditionOccurrence;
			// in case this condition is following an event we are done with the search
			passed = ( thread->type == EventOccurrence ) ? 0 : test_condition( occurrence, context );
			if ( passed ) {
				todo |= search_and_register_events( instance, log, occurrence, context );
			}
			break;
		case EventOccurrence:
			last = EventOccurrence;
			passed = 1;
			if ( !occurrence->registered ) {
				passed = test_event( instance, log, occurrence, context );
				if ( passed ) {
					// register occurrence in context->frame.events
					addItem( &instance->frame.events, occurrence );
					occurrence->registered = 1;
				}
			}
			if ( passed ) {
				todo |= search_and_register_events( instance, log, occurrence, context );
			}
			break;
		case ActionOccurrence:
			passed = 1;
			todo = 1;
			break;

		case ThenOccurrence: // should be impossible
			passed = 1;
			break;
		}
		context->narrative.occurrence = NULL;
	}
	return todo;
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
	OccurrenceType last;
	int passed = 1;
	for ( listItem *i = thread->sub.n; i!=NULL; i=i->next )
	{
		Occurrence *occurrence = (Occurrence *) i->ptr;
		context->narrative.occurrence = occurrence;

		switch ( occurrence->type ) {
		case OtherwiseOccurrence:
			if ( passed )	// last pass's result
				break;
			passed = 1;	// this pass's result, so far
			listItem *i = occurrence->va;
			if ( i == NULL ) {
				// if last was a ConditionOccurrence, then it cannot have passed,
				// therefore this standalone else would pass
				passed = (( last == ConditionOccurrence ) || occurrence->registered );
			} else {
				Occurrence *o = i->ptr;
				last = o->type;
				if ( o->type == EventOccurrence )
					passed = occurrence->registered;
				// must evaluate otherwise conditions for this frame
				else {
					for ( ; (i!=NULL) && passed; i=i->next ) {
						passed = test_condition( i->ptr, context );
					}
				}
			}
			if ( passed ) {
				occurrence->registered = 0; // no need to take the same route twice
				search_and_register_actions( narrative, occurrence, context );
			}
			break;
		case ConditionOccurrence:
			last = ConditionOccurrence;
			passed = test_condition( occurrence, context );
			if ( passed ) {
				search_and_register_actions( narrative, occurrence, context );
			}
			break;
		case EventOccurrence:
			last = EventOccurrence;
			passed = occurrence->registered;
			if ( passed ) {
				occurrence->registered = 0; // no need to take the same route twice
				search_and_register_actions( narrative, occurrence, context );
			}
			break;
		case ActionOccurrence:
			passed = 1;
			if ( !occurrence->registered ) {
				occurrence->registered = 1;
				addItem( &narrative->frame.actions, occurrence );
			}
			break;
		case ThenOccurrence:
			passed = 1;
			break;
		}
		context->narrative.occurrence = NULL;
	}
}

/*---------------------------------------------------------------------------
	execute_actions
---------------------------------------------------------------------------*/
static int
execute_actions( Narrative *instance, _context *context )
{
        push( base, 0, &same, context );
	context->narrative.level = context->command.level + 1;

	for ( listItem *i = instance->frame.actions; i!=NULL; i=i->next )
	{
		Occurrence *occurrence = (Occurrence *) i->ptr;
		if ( !occurrence->registered ) continue;

		occurrence->registered = 0;

		// set context current action for lookupVariable
		context->narrative.occurrence = occurrence;

		// execute action vector - currently singleton
		for ( listItem *j = occurrence->va; j!=NULL; j=j->next )
		{
			ActionVA *action = j->ptr;
			listItem *instructions = action->instructions;
			if ( instructions->next == NULL ) {
				cn_do( instructions->ptr );
			} else {
				push( base, 0, &same, context );
				int level = context->command.level;
				cn_dob( instructions );
				if ( context->command.level == level ) {
					// returned from '/'
					pop( base, 0, &same, context );
				}
			}
		}

		context->narrative.occurrence = NULL;
		if ( instance->deactivate )
			break;

		// action is done: remove parent ThenOcurrence from current narrative then list
		for ( Occurrence *thread = occurrence->thread; thread!=NULL; thread=thread->thread )
		{
			if ( thread->type != ThenOccurrence )
				continue;

			thread->registered = 0;
			listItem *last_j = NULL, *next_j;
			for ( listItem *j = instance->frame.then; j!=NULL; j=next_j ) {
				next_j = j->next;
				if ( j->ptr == thread ) {
					clipListItem( &instance->frame.then, j, last_j, next_j );
					break;
				} else last_j = j;
			}
			break; // no need to further loop
		}

		// look for a ThenOccurrence child - there would be at most one
		Occurrence *then = ((occurrence->sub.n) ? occurrence->sub.n->ptr : NULL );

		// failing that, look for a ThenOccurrence sibling from parent thread
		for ( Occurrence *thread = occurrence->thread; (thread) && !(then); thread=thread->thread )
		{
			listItem *j = thread->sub.n;
			// we know that 'then' occurrences are last born
			while ( j->next != NULL ) j=j->next;
			then = j->ptr;
			if ( then->type != ThenOccurrence )
				{ then = NULL; continue; }
			if ( then->registered )
				{ then = NULL; break; }
			// verify that this sibling is not in fact our parent
			for ( Occurrence *t = occurrence->thread; t!=thread; t=t->thread )
				if ( t == then ) { then = NULL; break; }
		}

		// register that then if found and not already registered
		if ( (then) && !then->registered ) {
			then->registered = 1;
			addItem( &instance->frame.then, then );
			// one-time event in case there are unconditional actions following
			addItem( &instance->frame.events, then );
		}
	}
	context->narrative.level = 0;
        pop( base, 0, &same, context );
	return 0;
}

/*---------------------------------------------------------------------------
	narrative_process
---------------------------------------------------------------------------*/
void
narrative_process( Narrative *n, EntityLog *log, _context *context )
{
	context->narrative.current = n;

	// perform actions registered from last frame - including possibly exit
	// --------------------------------------------------------------------
#ifdef DEBUG
	output( Debug, "narrative_process: 1. invoking execute_actions()" );
#endif
	// reorder the actions to execute them in FIFO order
	reorderListItem( &n->frame.actions );
	execute_actions( n, context );
	freeListItem( &n->frame.actions );

	// check if the narrative reached 'exit'
	if ( n->deactivate ) {
		context->narrative.current = NULL;
		return;
	}

	// translate events registered last frame into new actions, based on current conditions
	// ------------------------------------------------------------------------------------
#ifdef DEBUG
	output( Debug, "narrative_process: 2. translate events into actions" );
#endif
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

	// Transform latest occurrences into active narrative events, based on current conditions
	// --------------------------------------------------------------------------------------
#ifdef DEBUG
	output( Debug, "narrative_process: 3. translate occurrences into events" );
#endif
	if ((n->frame.then))
	{
		reorderListItem( &n->frame.then );
		for ( listItem *j = n->frame.then; j!=NULL; j=j->next )
		{
			Occurrence *then = (Occurrence *) j->ptr;
			search_and_register_events( n, log, then, context );
			then->registered = 0;
		}
	}
	else
	{
		int todo = search_and_register_events( n, log, &n->root, context );

		// in case we have found action(s) without trigger, then we must
		// trigger the next frame's search_and_register_actions()
		// Note that we could do the same for then occurrences - provided
		// we wanted to stay in 'then' state unless explicitely specified.
		// But for now execute_narrative_action() pops after execution.

		if ( todo && !(n->frame.events) && !(n->frame.actions) ) {
			search_and_register_actions( n, &n->root, context );
		}
	}

	// Keep only those events which the upstream events allow
	filter_narrative_events( n, context );

	context->narrative.current = NULL;
}

/*---------------------------------------------------------------------------
	frame_process, frame_start, frame_finish
---------------------------------------------------------------------------*/
static EntityLog *
frame_start( _context *context )
{
	EntityLog *log = context->frame.log.entities.backbuffer;

	reorderListItem( &log->instantiated );
	reorderListItem( &log->released );
	reorderListItem( &log->activated );
	reorderListItem( &log->deactivated );

	// swap frame buffers
	context->frame.log.entities.backbuffer = context->frame.log.entities.frontbuffer;
	context->frame.log.entities.frontbuffer = log;

	// Check entity narratives to be deactivated - and deactivate them
#ifdef DEBUG
	output( Debug, "frame_start: deactivate narratives" );
#endif
	Registry *registry = &context->frame.log.narratives.deactivate;
	for ( registryEntry *i = registry->value; i!=NULL; i=i->next )
	{
		Narrative *narrative = (Narrative *) i->index.address;
		listItem *entities = (listItem *) i->value;
		for ( listItem *j = entities; j!=NULL; j=j->next )
			deactivateNarrative( (Entity *) j->ptr, narrative );
		freeListItem( (listItem **) &i->value );
	}
	emptyRegistry( registry );

	return log;
}

static void
frame_process( EntityLog *log, _context *context )
{
#ifdef DEBUG
	static int frameNumber = 0;
	outputf( Debug, "entering frame_process: frame#=%d", frameNumber++ );
#endif
	// Have each active narrative process the frame log
	for ( listItem *i = context->narrative.registered; i!=NULL; i=i->next )
	{
		Narrative *narrative = (Narrative *) i->ptr;
		for ( registryEntry *j = narrative->instances.value, *next_j; j!=NULL; j=next_j )
		{
			next_j = j->next; // needed in case of deactivation
			Entity *e = (Entity *) j->index.address;
			Narrative *n = (Narrative *) j->value;
			narrative_process( n, log, context );
			if ( n->deactivate )
				deactivateNarrative( e, narrative );
		}
	}
#ifdef DEBUG
	output( Debug, "leaving frame_process" );
#endif
}

static void
frame_finish( EntityLog *log, _context *context )
{
	// Check narratives to be activated - and activate them
#ifdef DEBUG
	output( Debug, "frame_finish: activate narratives" );
#endif
	Registry *registry = &context->frame.log.narratives.activate;
	for ( registryEntry *i = registry->value; i!=NULL; i=i->next )
	{
		Narrative *narrative = (Narrative *) i->index.address;
		listItem *entities = (listItem *) i->value;
		for ( listItem *j = entities; j!=NULL; j=j->next )
			activateNarrative( (Entity *) j->ptr, narrative );
		freeListItem( (listItem **) &i->value );
	}
	emptyRegistry( registry );

	// update backlog counter for io_scan()
	context->frame.backlog =
		test_log( context, OCCURRENCES ) ? 3 :
		test_log( context, EVENTS ) ? 2 :
		test_log( context, ACTIONS ) ? 1 : 0 ;

	io_notify( log, context );

	for ( listItem *i=log->deactivated; i!=NULL; i=i->next )
		freeLiteral( i->ptr );

	freeListItem( &log->deactivated );
	freeListItem( &log->instantiated );
	freeListItem( &log->released );
	freeListItem( &log->activated );
}

/*---------------------------------------------------------------------------
	frame_update
---------------------------------------------------------------------------*/
int
frame_update( char *state, int event, char **next_state, _context *context )
{
	if ( context->frame.updating )
		return event;
	if ( !command_mode( 0, 0, ExecutionMode ) )
		return event;
	if ( event || strcmp( state, base ) )
		return event;

	context->frame.updating = 1;

	// make it triple, so that occurrences and events can travel
	// all the way to actions - and those executed
	for ( int i=0; i<3; i++ )
	{
		EntityLog *log = frame_start( context );
		frame_process( log, context );
		frame_finish( log, context );
		if ( !context->frame.backlog )
			break; // optimization
	}

	context->frame.updating = 0;
	return event;
}

