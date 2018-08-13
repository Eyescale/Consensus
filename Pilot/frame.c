#include <stdio.h>
#include <stdlib.h>

#include <stdarg.h>
#include <string.h>
#include <strings.h>

#include "list.h"
#include "registry.h"
#include "database.h"
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
	test_frame_log
---------------------------------------------------------------------------*/
int
test_frame_log( _context *context, LogType type )
{
	switch ( type ) {
	case OCCURRENCES:
		; CNLog *log = context->frame.log.entities.backbuffer;
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
		// not used, as execute_actions() registers then occurrences as events
		for ( listItem *i = context->narrative.registered; i!=NULL; i=i->next ) {
			Narrative *n = (Narrative *) i->ptr;
			if (( n->frame.then )) return 1;
		}
		return 0;
	}
}

/*---------------------------------------------------------------------------
	frame_update
---------------------------------------------------------------------------*/
static CNLog *frame_start( _context * );
static void frame_process( CNLog *, _context * );
static void frame_finish( CNLog *, _context * );

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

	// first inject CGI input occurrences
	CNLog *log = &context->frame.log.cgi;
	if (( log->instantiated )) {
#if 0
		Narrative *n = lookupNarrative( CN.nil, "cgi" );
		if (( n )) {
			activateNarrative( CN.nil, n );
			narrative_process( n, log, context );
			while ( !n->deactivate && ((n->frame.events) || (n->frame.actions) || (n->frame.then))) {
				narrative_process( n, NULL, context );
			}
			deactivateNarrative( CN.nil, n );
		}
#else
		frame_process( log, context );
#endif
		freeLiterals( &log->instantiated );
	}

	// general processing - make it triple, so that occurrences and events may
	// travel all the way to actions - and those executed
	for ( int i=0; i<3; i++ )
	{
#ifdef DEBUG
		if ( context->control.session )
			fprintf( stderr, "FRAME_UPDATE[ %d ] BGN: backlog=%d - ", i, context->frame.backlog );
#endif
		log = frame_start( context );
		frame_process( log, context );
		frame_finish( log, context );
#ifdef DEBUG
		if ( context->control.session )
			fprintf( stderr, "END: backlog=%d\n", context->frame.backlog );
#endif
		if ( !context->frame.backlog )
			break; // optimization
	}

	context->frame.updating = 0;
	return 0;
}

static CNLog *
frame_start( _context *context )
{
	CNLog *log = context->frame.log.entities.backbuffer;

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
frame_process( CNLog *log, _context *context )
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
			if ( n->deactivate ) {
#ifdef DEBUG
				output( Debug, "frame_process: deactivating" );
#endif
				deactivateNarrative( e, narrative );
			}
		}
	}
#ifdef DEBUG
	output( Debug, "leaving frame_process" );
#endif
}

static void
frame_finish( CNLog *log, _context *context )
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
		test_frame_log( context, OCCURRENCES ) ? 3 :
		test_frame_log( context, EVENTS ) ? 2 :
		test_frame_log( context, ACTIONS ) ? 1 : 0 ;

	io_notify( log, context );

	freeListItem( &log->instantiated );
	freeLiterals( &log->released );
	freeListItem( &log->activated );
	freeListItem( &log->deactivated );
}

/*---------------------------------------------------------------------------
	narrative_process
---------------------------------------------------------------------------*/
static int execute_actions( Narrative *, _context * );
static void search_and_register_actions( Narrative *, Occurrence *, _context * );
static int search_and_register_events( Narrative *, CNLog *, Occurrence *, _context *);
static void filter_narrative_events( Narrative *, _context * );

void
narrative_process( Narrative *n, CNLog *log, _context *context )
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
	execute_actions
---------------------------------------------------------------------------*/
static int
execute_actions( Narrative *instance, _context *context )
{
        push( base, 0, &same, context );
	context->narrative.level = context->command.level + 1;

	for ( listItem *i = instance->frame.actions; i!=NULL; i=i->next ) {
		Occurrence *occurrence = (Occurrence *) i->ptr;
		if ( !occurrence->registered ) continue;

		occurrence->registered = 0;

		// set context current action for lookupVariable
		context->narrative.occurrence = occurrence;

		// execute action vector - currently singleton
		for ( listItem *j = occurrence->va; j!=NULL; j=j->next ) {
			ActionVA *action = j->ptr;
			listItem *instructions = action->instructions;
			if ( instructions->next == NULL ) {
#ifdef DEBUG
				if ( context->control.session )
					outputf( Debug, "EXECUTING: %s", instructions->ptr );
#endif
				cn_do( instructions->ptr );
			}
			else cn_dob( instructions, 0 );
		}

		context->narrative.occurrence = NULL;
		if ( instance->deactivate )
			break;

		// action is done: remove parent ThenOcurrence from narrative frame's then list
		for ( Occurrence *thread = occurrence->thread; thread!=NULL; thread=thread->thread ) {
			if ( thread->type != ThenOccurrence ) continue;
			thread->registered = 0;
			listItem *last_j = NULL, *next_j;
			for ( listItem *j = instance->frame.then; j!=NULL; j=next_j ) {
				next_j = j->next;
				if ( j->ptr == thread ) {
					clipListItem( &instance->frame.then, j, last_j, next_j );
					break;
				}
				else last_j = j;
			}
			break; // no need to further loop
		}

		// look for a ThenOccurrence child - there is at most one
		Occurrence *then = ((occurrence->sub.n) ? occurrence->sub.n->ptr : NULL );

		// failing that, look for a ThenOccurrence sibling from parent thread
		for ( Occurrence *thread = occurrence->thread; (thread) && !(then); thread=thread->thread ) {
			listItem *j = thread->sub.n;
			// we know that 'then' occurrences are last born
			while ( j->next != NULL ) j=j->next;
			then = j->ptr;
			if ( then->type != ThenOccurrence )
				{ then = NULL; continue; }
			if ( then->registered )
				{ then = NULL; break; }
			// verify that this sibling is not in our direct ancestry
			for ( Occurrence *t = occurrence->thread; t!=thread; t=t->thread )
				if ( t == then ) { then = NULL; break; }
		}

		// DO_LATER: the following assumes there are no active sibling threads in our ancestry
		if (( then )) {
			if ( !then->registered ) {
				then->registered = 1;
				freeVariables( &then->variables );
				addItem( &instance->frame.then, then );
				// one-time event in case there are unconditional actions following
				addItem( &instance->frame.events, then );
			}
			// transfer current action's variables over to then occurrence
			transferVariables( &then->variables, &occurrence->variables, 1 );
			// if not our descendant, transfer ancestors' variables over to then occurrence
			if ( occurrence->sub.n == NULL ) {
				for ( Occurrence *t = occurrence->thread; t!=NULL; t=t->thread ) {
					if ( t->type != ThenOccurrence ) continue;
					transferVariables( &then->variables, &t->variables, 0 );
				}
			}
		}
		else {
			freeVariables( &occurrence->variables );
			for ( Occurrence *t = occurrence->thread; t!=NULL; t=t->thread )
				if ( t->type == ThenOccurrence ) freeVariables( &t->variables );
		}
	}
	context->narrative.level = 0;
        pop( base, 0, &same, context );
	return 0;
}

/*---------------------------------------------------------------------------
	search_and_register_actions	- recursive
---------------------------------------------------------------------------*/
static int test_condition( Occurrence *, _context * );

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
	search_and_register_events	- recursive
---------------------------------------------------------------------------*/
static int test_event( Narrative *, CNLog *, Occurrence *, _context * );

static int
search_and_register_events( Narrative *instance, CNLog *log, Occurrence *thread, _context *context )
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
	test_condition
---------------------------------------------------------------------------*/
static void register_results( Occurrence *, char *identifier, _context * );

static int
test_condition( Occurrence *occurrence, _context *context )
{
	int retval = ( occurrence->contrary ? 0 : 1 );
	for ( listItem *i = occurrence->va; i!=NULL; i=i->next )
	{
		ConditionVA *condition = (ConditionVA *) i->ptr;
		context->expression.mode = 0;
		int success = expression_solve( condition->format, 3, NULL, context );
		register_results( occurrence, condition->identifier, context );
		if ( success <= 0 ) {
			retval = ( occurrence->contrary ? 1 : 0 );
			break;
		}
	}
	return retval;
}

static void
register_results( Occurrence *occurrence, char *identifier, _context *context )
/*
	register condition or event test results in occurrence->variables
*/
{
	if (( identifier )) {
		int type = ( context->expression.mode == ReadMode ) ? LiteralResults : EntityResults;
		assign_occurrence_variable( identifier, context->expression.results, type, context );
		context->expression.results = NULL;
	}
	else if ( context->expression.mode == ReadMode ) {
		freeLiterals( &context->expression.results );
	}
	else freeListItem( &context->expression.results );
}

/*---------------------------------------------------------------------------
	test_event
---------------------------------------------------------------------------*/
static int
test_event( Narrative *instance, CNLog *log, Occurrence *occurrence, _context *context )
{
	for ( listItem *i = occurrence->va; i!=NULL; i=i->next )
	{
		EventVA *event = (EventVA *) i->ptr;
		switch ( event->source.scheme ) {
		case DefaultScheme:
			break;
		case CGIScheme:
			if ( log != &context->frame.log.cgi )
				return 0;
			break;
		default:
			// DO_LATER
			return 0;
		}
		listItem *loglist;
		switch ( event->type ) {
		case InstantiateEvent:
			loglist = log->instantiated;
			break;
		case ReleaseEvent:
			loglist = log->released;
			break;
		case ActivateEvent:
			loglist = log->activated;
			break;
		case DeactivateEvent:
			loglist = log->deactivated;
			break;
		default:
			return 0;
		}
		if ( loglist == NULL ) return 0;
		context->expression.mode =
			( event->source.scheme || ( event->type == ReleaseEvent )) ?
			ReadMode : EvaluateMode;
		int success = expression_solve( event->format, 3, loglist, context );
		register_results( occurrence, event->identifier, context );
		if ( success <= 0 ) return 0;
	}
	return 1;
}

/*---------------------------------------------------------------------------
	filter_narrative_events
---------------------------------------------------------------------------*/
static int test_upstream_events( Occurrence *event );
static void deregister_events( Occurrence *event );

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

