#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>

#include "database.h"
#include "registry.h"
#include "kernel.h"

#include "api.h"
#include "input.h"
#include "input_util.h"
#include "output.h"
#include "path.h"
#include "command.h"
#include "expression.h"
#include "expression_util.h"
#include "narrative.h"
#include "narrative_util.h"

// #define DEBUG
// #define DEBUG2

/*---------------------------------------------------------------------------
	narrative engine
---------------------------------------------------------------------------*/
#define do_( a, s )  \
	event = narrative_execute( a, &state, event, s, context );
/*
   stack->narrative.state.whole		true if an action has been specified at a higher stack level
   stack->narrative.state.event		true if an event has been specified at this stack level
   stack->narrative.state.condition	true if a condition has been specified at this stack level
   stack->narrative.state.action	true if an action has been specified at this stack level
   stack->narrative.state.then		true if a then has been specified at this stack level
  
   the idea is to pop until we find the first lower stack level which is not a then
*/
static int
narrative_pop( char *state, int event, char **next_state, _context *context )
{
	StackVA *stack = (StackVA *) context->command.stack->ptr;
	int whole = stack->narrative.state.whole;

	if ( !whole && ( context->narrative.level < context->command.level )) {
		output( Warning, "last narrative statement incomplete - will be truncated or removed..." );
	}
	for ( ; ; )
	{
		event = pop( state, event, next_state, context );
		if ( event < 0 ) return event;

		if ( context->command.level < context->narrative.level )
		{
			*next_state = "";
			return event;
		}

		stack = (StackVA *) context->command.stack->ptr;
		if ( stack->narrative.state.then )
		{
			/*
			   if the user did not specify an action, and
			   the stack state is then
			*/
			if ( !whole )
			{
				/*
				   then not followed by action is void
				*/
				stack->narrative.state.then = 0;
				/*
				   then was preceded by an action, whose flag
				   we overrode when setting up the then
				*/
				stack->narrative.state.action = 1;
				/*
				   if this then did not start a new line
				   ... then we can stay there
				*/
				if ( stack->narrative.state.whole ) {
					stack->narrative.thread = NULL;
					return event;
				}
			}
		}
		else if ( stack->narrative.state.event || stack->narrative.state.condition || stack->narrative.state.action )
		{
			stack->narrative.thread = NULL;
			return event;
		}
	}
}

static int
narrative_execute( _action action, char **state, int event, char *next_state, _context *context )
{
	if ( !strcmp( next_state, pop_state ) )
	{
		// int retval = action( *state, event, &next_state, context );
		event = narrative_pop( *state, event, &next_state, context );
		*state = next_state;
	}
	else
	{
		if ( action == flush_input ) {
			StackVA *stack = (StackVA *) context->command.stack->ptr;
			stack->narrative.thread = NULL;
		}
		event = action( *state, event, &next_state, context );
                if ( strcmp( next_state, same ) )
                        *state = next_state;
	}
	return event;
}

/*---------------------------------------------------------------------------
	debug utility
---------------------------------------------------------------------------*/
#ifdef DEBUG2
static _action push_narrative_condition, push_narrative_event, read_narrative_then, push_narrative_then, set_narrative_action;
static void debug_stack( StackVA *stack, _action from )
{
	if ( from == push_narrative_condition ) {
		output( Debug, "narrative: ( %d, %d, %d, %d )-in->( 1, %d, %d, %d )",
		stack->narrative.state.condition, stack->narrative.state.event, stack->narrative.state.action, stack->narrative.state.then,
		stack->narrative.state.condition, stack->narrative.state.action, stack->narrative.state.then );
	} else if ( from == push_narrative_event ) {
		output( Debug, "narrative: ( %d, %d, %d, %d )-on->( %d, 1, %d, %d )",
		stack->narrative.state.condition, stack->narrative.state.event, stack->narrative.state.action, stack->narrative.state.then,
		stack->narrative.state.event, stack->narrative.state.action, stack->narrative.state.then );
	} else if ( from == read_narrative_then ) {
		output( Debug, "narrative: ( %d, %d, %d, %d )-then->( 0, 0, 0, 1 )",
		stack->narrative.state.condition, stack->narrative.state.event, stack->narrative.state.action, stack->narrative.state.then );
		break;
	} else if ( from == push_narrative_then ) {
		output( Debug, "narrative: pushing ( %d, %d, %d, %d )-then->( %d, %d, %d, 1 )",
		stack->narrative.state.condition, stack->narrative.state.event, stack->narrative.state.action, stack->narrative.state.then,
		stack->narrative.state.condition, stack->narrative.state.event, stack->narrative.state.action );
	} else if ( from == set_narrative_action ) {
		output( Debug, "narrative: ( %d, %d, %d, %d )-do->( %d, %d, 1, %d )",
		stack->narrative.state.condition, stack->narrative.state.event, stack->narrative.state.action, stack->narrative.state.then,
		stack->narrative.state.condition, stack->narrative.state.event, stack->narrative.state.then );
	}
}
#endif

/*---------------------------------------------------------------------------
	utility functions
---------------------------------------------------------------------------*/
static Occurrence *
retrieve_narrative_thread( _context *context )
{
	StackVA *stack = (StackVA *) context->command.stack->ptr;
	Occurrence *thread = stack->narrative.thread;
	if ( thread == NULL ) {
		if ( context->command.level == context->narrative.level ) {
			thread = &context->narrative.current->root;
		}
		else {
			StackVA *s = (StackVA *) context->command.stack->next->ptr;
			thread = s->narrative.thread;
		}
	}
	return thread;
}

static int
narrative_build( Occurrence *occurrence, int event, _context *context )
{
	StackVA *stack = (StackVA *) context->command.stack->ptr;

	// retrieve and set parent occurrence - aka. narrative thread
	Occurrence *parent = retrieve_narrative_thread( context );
	occurrence->thread = parent;

#ifdef DEBUG
	output( Debug, "building narrative: thread %0x", (int) parent );
#endif
	if ( stack->narrative.state.otherwise ) {
		// add occurrence to the "otherwise" parent's list
		addItem( &parent->va, occurrence );
		parent->va_num++;
		stack->narrative.state.otherwise = 0;
	} else {
		// add occurrence to the normal parent's children list
		addItem( &parent->sub.n, occurrence );
		parent->sub.num++;

		// update narrative thread - note: actions cannot have children
		if (( occurrence->type == ActionOccurrence ) && (event == '\n' )) {
			stack->narrative.thread = NULL;
		} else {
			stack->narrative.thread = occurrence;
		}
	}
	return 0;
}

static int
narrative_on_in_on_( int event, _context *context )
{
	StackVA *stack = (StackVA *) context->command.stack->ptr;
	Occurrence *parent = stack->narrative.thread;
	if ( parent == NULL ) {
		if ( context->command.level == context->narrative.level ) {
			return event;	// no problem
		} else {
			parent = ((StackVA *) context->command.stack->next->ptr)->narrative.thread;
		}
	}
	for ( ; parent != NULL; parent=parent->thread ) {
		if ( parent->type == ThenOccurrence ) {
			parent = NULL;
			break;
		}
		if ( parent->type == ConditionOccurrence )
			break;
	}
	for ( ; parent != NULL; parent=parent->thread ) {
		if ( parent->type == ThenOccurrence )
			break;
		if ( parent->type == EventOccurrence ) {
			return output( Error, "narrative on-in-on sequence is not supported" );
		}
	}
	return event;
}

static int
narrative_on_init_on_( int event, _context *context )
{
	StackVA *stack = (StackVA *) context->command.stack->ptr;
	Occurrence *parent = stack->narrative.thread;
	if ( parent == NULL ) {
		if ( context->command.level == context->narrative.level ) {
			return event;	// no problem
		} else {
			parent = ((StackVA *) context->command.stack->next->ptr)->narrative.thread;
		}
	}
	for ( ; parent != NULL; parent=parent->thread ) {
		if ( parent->type == ThenOccurrence )
			break;
		if ( parent->type == EventOccurrence ) {
			EventVA *e = (EventVA *) parent->va->ptr;
			if ( e->type == InitEvent ) {
				return output( Error, "narrative on-init-on sequence is not supported" );
			}
		}
	}
	return event;
}

static int
narrative_on_init_( int event, _context *context )
{
	if ( context->command.level > context->narrative.level ) {
		return output( Error, "narrative on-init sequence must start from base" );
	}
	return event;
}

/*---------------------------------------------------------------------------
	narrative_condition actions
---------------------------------------------------------------------------*/
static int
read_narrative_in( char *state, int event, char **next_state, _context *context )
{
	state = "i";
	do {
		event = read_input( state, 0, NULL, context );
		bgn_
		in_( "i" ) bgn_
			on_( 'n' )	do_( nop, "in" )
			on_other	do_( error, "" )
			end
			in_( "in" ) bgn_
				on_( ' ' )	do_( nothing, "" )
				on_( '\t' )	do_( nothing, "" )
				on_( '(' )	do_( nothing, "" )
				on_other	do_( error, "" )
				end
		end
	}
	while ( strcmp( state, "" ) );

	return event;
}
static int
read_narrative_condition( char *state, int event, char **next_state, _context *context )
{
	freeExpression( context->expression.ptr );
	context->expression.ptr = NULL;
	free( context->identifier.id[ 0 ].ptr );
	context->identifier.id[ 0 ].ptr = NULL;

	state = base;
	do {
		event = read_input( state, event, NULL, context );
		bgn_
		on_( 0 )	do_( error, "" )
		on_( -1 )	do_( nothing, "" )
		in_( base ) bgn_
			on_( ':' )	do_( nop, "identifier:" )
			on_other	do_( read_0, "identifier" )
			end
			in_( "identifier" ) bgn_
				on_( ' ' )	do_( nop, same )
				on_( '\t' )	do_( nop, same )
				on_( ':' )	do_( nop, "identifier:" )
				on_other	do_( error, base )
				end
			in_( "identifier:" ) bgn_
				on_any		do_( read_expression, "" )
				end
		end
	}
	while ( strcmp( state, "" ) );

	return event;
}

static int
narrative_condition_begin( char *state, int event, char **next_state, _context *context )
{
	Occurrence *occurrence = newOccurrence( ConditionOccurrence );

	StackVA *stack = (StackVA *) context->command.stack->ptr;
	int backup = stack->narrative.state.otherwise;
	event = narrative_build( occurrence, event, context );
	stack->narrative.state.otherwise = backup;

	return event;
}

static ConditionVA *
set_condition_value( Occurrence *occurrence, _context *context )
{
	ConditionVA *condition = (ConditionVA *) calloc( 1, sizeof(ConditionVA) );
	addItem( &occurrence->va, condition );
	occurrence->va_num++;
	condition->format = context->expression.ptr;
	context->expression.ptr = NULL;
	if ( context->identifier.id[ 0 ].ptr != NULL ) {
		condition->identifier = context->identifier.id[ 0 ].ptr;
		context->identifier.id[ 0 ].ptr = NULL;
	}
	return condition;
}

static int
narrative_condition_add( char *state, int event, char **next_state, _context *context )
{
	StackVA *stack = (StackVA *) context->command.stack->ptr;
	Occurrence *thread = retrieve_narrative_thread( context );
	if ( stack->narrative.state.otherwise ) {
		thread = thread->va->ptr;
	}
	set_condition_value( thread, context );
	return 0;
}

static int
narrative_condition_end( char *state, int event, char **next_state, _context *context )
{
	StackVA *stack = (StackVA *) context->command.stack->ptr;
	Occurrence *thread = retrieve_narrative_thread( context );
	if ( stack->narrative.state.otherwise ) {
		thread = thread->va->ptr;
	}
	set_condition_value( thread, context );
	reorderListItem( &thread->va );
	stack->narrative.state.otherwise = 0;
	return 0;
}

static int
set_narrative_condition( char *state, int event, char **next_state, _context *context )
{
	Occurrence *occurrence = newOccurrence( ConditionOccurrence );
	occurrence->type = ConditionOccurrence;
	set_condition_value( occurrence, context );
	return narrative_build( occurrence, event, context );
}

static int
push_narrative_condition( char *state, int event, char **next_state, _context *context )
{
	StackVA *stack = (StackVA *) context->command.stack->ptr;
	if ( !stack->narrative.state.then ) {
#ifdef DEBUG2
		debug_stack( stack, push_narrative_condition );
#endif
		stack->narrative.state.condition = 1;
	}
	event = push( state, event, next_state, context );
	*next_state = base;
	return event;
}

/*---------------------------------------------------------------------------
	narrative_event actions
---------------------------------------------------------------------------*/
static int
read_narrative_on( char *state, int event, char **next_state, _context *context )
{
	state = "o";
	do {
		event = read_input( state, 0, NULL, context );
		bgn_
		in_( "o" ) bgn_
			on_( 'n' )	do_( nop, "on" )
			on_other	do_( error, "" )
			end
			in_( "on" ) bgn_
				on_( ' ' )	do_( nothing, "" )
				on_( '\t' )	do_( nothing, "" )
				on_( '(' )	do_( nothing, "" )
				on_other	do_( error, "" )
				end
		end
	}
	while ( strcmp( state, "" ) );

	if  ( event > 0 ) {
		event = narrative_on_in_on_( event, context );
	}
	if ( event > 0 ) {
		event = narrative_on_init_on_( event, context );
	}
	return event;
}

static int
set_event_identifier( char *state, int event, char **next_state, _context *context )
{
	if ( !strcmp( context->identifier.id[ 0 ].ptr, "init" ) ) {
		return output( Error, "'init' used as event identifier" );
	}
	StackVA *stack = (StackVA *) context->command.stack->ptr;
	stack->narrative.event.identifier = context->identifier.id[ 0 ].ptr;
	context->identifier.id[ 0 ].ptr = NULL;
	return 0;
}

static int
set_event_type( char *state, int event, char **next_state, _context *context )
{
	StackVA *stack = (StackVA *) context->command.stack->ptr;
	stack->narrative.event.type =
		( event == '<' ) ? StreamEvent :
		( event == '!' ) ? InstantiateEvent :
		( event == '~' ) ? ReleaseEvent :
		( event == '*' ) ? ActivateEvent :
		( event == '_' ) ? DeactivateEvent : 0;

	return 0;
}

static int
check_init_event( char *state, int event, char **next_state, _context *context )
{
	if ( !strcmp( context->identifier.id[ 0 ].ptr, "init" ) ) {
		event = narrative_on_init_( event, context );
		if ( event != -1 ) {
			StackVA *stack = (StackVA *) context->command.stack->ptr;
			stack->narrative.event.type = InitEvent;
		}
	}
	else {
		event = outputf( Error, "unspecified event for identifier '%s'", context->identifier.id[ 0 ].ptr );
	}
	return event;
}

static int
read_narrative_event( char *state, int event, char **next_state, _context *context )
{
	freeExpression( context->expression.ptr );
	context->expression.ptr = NULL;

	StackVA *stack = (StackVA *) context->command.stack->ptr;
	bzero( &stack->narrative.event, sizeof( EventVA ) );
	state = base;
	do {
		event = read_input( state, event, NULL, context );
#ifdef DEBUG
		output( Debug, "read_narrative_event: in \"%s\", on '%c'", state, event );
#endif
		bgn_
		on_( 0 )	do_( error, "" )
		on_( -1 )	do_( error, "" )
		on_( ' ' )	do_( nop, same )
		on_( '\t' )	do_( nop, same )
		in_( base ) bgn_
			on_( ':' )	do_( nop, "identifier:" )
			on_other	do_( read_0, "identifier" )
			end
			in_( "identifier" ) bgn_
				on_( ':' )	do_( set_event_identifier, "identifier:" )
				on_other	do_( check_init_event, "" )
				end
				in_( "identifier:" ) bgn_
					on_any	do_( read_expression, "identifier: expression" )
					end
					in_( "identifier: expression" ) bgn_
						on_( ':' )	do_( nop, "identifier: scheme:" )
						on_( '!' )	do_( nop, "identifier: expression !" )
						on_other	do_( error, base )
						end
					in_( "identifier: scheme:" ) bgn_
						on_any	do_( read_path, "identifier: scheme://path" )
						end
						in_( "identifier: scheme://path" ) bgn_
							on_( '!' )	do_( nop, "identifier: scheme://path !" )
							end
							in_( "identifier: scheme://path !" ) bgn_
								on_( '<' )	do_( set_event_type, "" )
								on_other	do_( error, base )
								end
					in_( "identifier: expression !" ) bgn_
						on_( '!' )	do_( set_event_type, "" )
						on_( '~' )	do_( set_event_type, "" )
						on_( '*' )	do_( set_event_type, "" )
						on_( '_' )	do_( set_event_type, "" )
						on_other	do_( error, base )
						end
		end
	}
	while ( strcmp( state, "" ) );

	return event;
}

static int
narrative_event_begin( char *state, int event, char **next_state, _context *context )
{
	StackVA *stack = (StackVA *) context->command.stack->ptr;
	Occurrence *occurrence = newOccurrence( EventOccurrence );
	int backup = stack->narrative.state.otherwise;
	event = narrative_build( occurrence, event, context );
	stack->narrative.state.otherwise = backup;
	return event;
}

static EventVA *
set_event_value( Occurrence *occurrence, EventVA *data, _context *context )
{
	EventVA *event = (EventVA *) calloc( 1, sizeof(EventVA) );
	addItem( &occurrence->va, event );
	occurrence->va_num++;
	memcpy( event, data, sizeof(EventVA) );
	if ( data->type == StreamEvent )
	{
		// in this case the event expression is a filepath
		event->format = context->identifier.id[ 1 ].ptr;
		context->identifier.id[ 1 ].ptr = NULL;
	}
	else
	{
		event->format = context->expression.ptr;
		context->expression.ptr = NULL;
	}
	return event;
}

static int
narrative_event_add( char *state, int event, char **next_state, _context *context )
{
	StackVA *stack = (StackVA *) context->command.stack->ptr;
	if ( stack->narrative.event.type == InitEvent ) {
		return output( Error, "Usage: 'on init' - parentheses not supported" );
	}
	Occurrence *thread = retrieve_narrative_thread( context );
	if ( stack->narrative.state.otherwise ) {
		thread = thread->va->ptr;
	}
	set_event_value( thread, &stack->narrative.event, context );
	return 0;
}

static int
narrative_event_end( char *state, int event, char **next_state, _context *context )
{
	StackVA *stack = (StackVA *) context->command.stack->ptr;
	event = narrative_event_add( state, event, next_state, context );
	Occurrence *thread = retrieve_narrative_thread( context );
	if ( stack->narrative.state.otherwise ) {
		thread = thread->va->ptr;
	}
	reorderListItem( &thread->va );
	stack->narrative.state.otherwise = 0;
	return event;
}

static int
set_narrative_event( char *state, int event, char **next_state, _context *context )
{
	StackVA *stack = (StackVA *) context->command.stack->ptr;
#ifdef DEBUG
	output( Debug, "set_narrative_event:\n"
	"\tidentifier: %s, type: %d\n"
	"\tinstantiate: %d, release: %d, activate: %d, deactivate: %d, notification: %d, request: %d, init: %d",
	stack->narrative.event.identifier.name, stack->narrative.event.identifier.type,
	stack->narrative.event.type.instantiate, stack->narrative.event.type.release, stack->narrative.event.type.activate,
	stack->narrative.event.type.deactivate, stack->narrative.event.type.notification, stack->narrative.event.type.request,
	stack->narrative.event.type.init );
#endif
	Occurrence *occurrence = newOccurrence( EventOccurrence );
	set_event_value( occurrence, &stack->narrative.event, context );

	return narrative_build( occurrence, event, context );
}

static int
push_narrative_event( char *state, int event, char **next_state, _context *context )
{
	StackVA *stack = (StackVA *) context->command.stack->ptr;
	if ( !stack->narrative.state.then ) {
#ifdef DEBUG2
		debug_stack( stack, push_narrative_event );
#endif
		stack->narrative.state.event = 1;
	}
	event = push( state, event, next_state, context );
	*next_state = base;
	return event;
}

/*---------------------------------------------------------------------------
	narrative_otherwise actions
---------------------------------------------------------------------------*/
static int
push_narrative_otherwise( char *state, int event, char **next_state, _context *context )
{
	if ( event < 0 ) return event;

	StackVA *stack = (StackVA *) context->command.stack->ptr;
	stack->narrative.state.otherwise = 0;

	event = push( state, event, next_state, context );
	*next_state = base;
	return event;
}

static int
set_narrative_otherwise( char *state, int event, char **next_state, _context *context )
{
	StackVA *stack = (StackVA *) context->command.stack->ptr;

	// check acceptance conditions
	if ( stack->narrative.state.otherwise || !stack->narrative.state.whole ) {
		goto ERROR;
	}
	Occurrence *parent = retrieve_narrative_thread( context );
	Occurrence *last = (Occurrence *) parent->sub.n->ptr;
	if ( last->sub.num > 1 ) {
		goto ERROR;
	}
	switch ( last->type ) {
	case EventOccurrence:
	case ConditionOccurrence:
		break;
	case OtherwiseOccurrence:
		if ( last->va == NULL )
			goto ERROR;
		OtherwiseVA *otherwise = (OtherwiseVA *) last->va->ptr;
		if ( otherwise->occurrence == NULL )
			goto ERROR;
		break;
	default:
		goto ERROR;
	}

	Occurrence *occurrence = newOccurrence( OtherwiseOccurrence );
	event = narrative_build( occurrence, event, context );
	stack->narrative.state.otherwise = 1;
	return event;
ERROR:
	return output( Error, "no narrative event or condition to alternate" );
}

/*---------------------------------------------------------------------------
	narrative_then actions
---------------------------------------------------------------------------*/
static int
read_narrative_then( char *state, int event, char **next_state, _context *context )
{
	StackVA *stack = (StackVA *) context->command.stack->ptr;
	if ( !stack->narrative.state.action && !stack->narrative.state.whole ) {
		return output( Error, "'then' must follow action..." );
	}
	if ( stack->narrative.state.exit ) {
		output( Warning, "'then' following exit - may be ignored" );
	}
	int read_from_base = !strcmp( state, base );
	event = 0;
	do_( on_token, "hen\0then" );
	bgn_
	in_( "then" ) do {
		event = read_input( state, 0, NULL, context );
		bgn_
			on_( ' ' )      do_( nothing, "" )
			on_( '\t' )     do_( nothing, "" )
			on_( '\n' )     do_( nothing, "" )
			on_other        do_( error, "" )
			end
		}
		while ( strcmp( state, "" ) );
        end
	if ( read_from_base && ( event != -1 ) )
	{
		StackVA *stack = (StackVA *) context->command.stack->ptr;
#ifdef DEBUG2
		debug_stack( stack, read_narrative_then );
#endif
		stack->narrative.state.event = 0;
		stack->narrative.state.condition = 0;
		stack->narrative.state.action = 0;
		stack->narrative.state.then = 1;
	}
	return event;
}

static int
restore_narrative_action( char *state, int event, char **next_state, _context *context )
{
	do_( error, same );
	StackVA *stack = (StackVA *) context->command.stack->ptr;
	stack->narrative.state.action = 1;
	return -1;
}

static int
set_narrative_then( char *state, int event, char **next_state, _context *context )
{
	StackVA *stack = (StackVA *) context->command.stack->ptr;
	if ( stack->narrative.state.otherwise ) {
		return outputf( Error, NULL );
	}
	Occurrence *occurrence = newOccurrence( ThenOccurrence );
	return narrative_build( occurrence, event, context );
}

static int
push_narrative_then( char *state, int event, char **next_state, _context *context )
{
	StackVA *stack = (StackVA *) context->command.stack->ptr;
#ifdef DEBUG2
	debug_stack( stack, push_narrative_then );
#endif
	stack->narrative.state.then = 1;
	event = push( state, event, next_state, context );
	*next_state = base;
	return event;
}

/*---------------------------------------------------------------------------
	narrative_action actions
---------------------------------------------------------------------------*/
static int
read_narrative_do( char *state, int event, char **next_state, _context *context )
{
	state = "d";
	do {
		event = read_input( state, 0, NULL, context );
		bgn_
		in_( "d" ) bgn_
			on_( 'o' )	do_( nop, "do" )
			on_other	do_( error, "" )
			end
			in_( "do" ) bgn_
				on_( ' ' )	do_( nothing, "" )
				on_( '\t' )	do_( nothing, "" )
				on_( '\n' )	do_( nothing, "" )
				on_other	do_( error, "" )
				end
		end
	}
	while ( strcmp( state, "" ) );

	return event;
}

static int
read_narrative_action( char *state, int event, char **next_state, _context *context )
{
	struct { int mode; } backup;
	backup.mode = context->command.mode;

	set_command_mode( InstructionMode, context );
	set_record_mode( OnRecordMode, event, context );
	event = command_read( NULL, InstructionOne, base, event, context );
	set_record_mode( OffRecordMode, event, context );
	set_command_mode( backup.mode, context );

#ifdef DEBUG
	output( Debug, "narrative: read action \"%s\" - returning '%c'", context->record.string.ptr, event );
#endif
	if ( event >= 0 ) {
		// Note: context->record.instructions is assumed to be NULL here...
		addItem( &context->record.instructions, context->record.string.ptr );
		context->record.string.ptr = NULL;
	}

	return event;
}

ActionVA *
set_action_value( Occurrence *occurrence, _context *context )
{
	listItem *instructions = context->record.instructions;
	context->record.instructions = NULL;

	ActionVA *action = (ActionVA *) calloc( 1, sizeof(ActionVA) );
	action->instructions = instructions;
	addItem( &occurrence->va, action );
	occurrence->va_num++;
	return action;
}

static int
set_narrative_action( char *state, int event, char **next_state, _context *context )
{
	StackVA *stack = (StackVA *) context->command.stack->ptr;
	Occurrence *occurrence;

	if ( !stack->narrative.state.then )
	{
	#ifdef DEBUG2
		debug_stack( stack, set_narrative_action );
	#endif
		int level = context->command.level;
		for ( listItem *s = context->command.stack; level>=context->narrative.level; s=s->next, level-- )
		{
			StackVA *sptr = (StackVA *) s->ptr;
			if ( sptr->narrative.state.whole ) break;
			sptr->narrative.state.whole = 1;
		}
	}
	stack->narrative.state.whole = 1;

	occurrence = newOccurrence( ActionOccurrence );
	ActionVA *action = set_action_value( occurrence, context );

	if (( action->instructions != NULL ) && !strcmp( action->instructions->ptr, "exit" )) {
		stack->narrative.state.exit = 1;
		action->exit = 1;
	}
	stack->narrative.state.otherwise = 0;
	int retval = narrative_build( occurrence, event, context );

	if ( stack->narrative.state.then && ( event == '\n' ) ) {
		do_( nothing, pop_state );
		*next_state = state;
	} else if ( retval >= 0 ) {
		stack->narrative.state.action = 1;
	}
	return retval;
}

static int
read_action_closure( char *state, int event, char **next_state, _context *context )
{
	int level = context->command.level;
	StackVA *stack = context->command.stack->ptr;
	stack->narrative.state.closure = 1;	// just for prompt
	do {
		event = read_input( state, 0, NULL, context );
		bgn_
		on_( 0 )	do_( error, "" )
		on_( -1 )	do_( nothing, "" )
		in_( base ) bgn_
			on_( ' ' )	do_( nop, same )
			on_( '\t' )	do_( nop, same )
			on_( '\n' )	do_( nop, same )
			on_( '.' )	do_( nop, "." )
			on_( 't' )	do_( nop, "t" )
			on_other	do_( nop, "flush" )
			end
			in_( "t" ) bgn_
				on_( 'h' )	do_( nop, "th" )
				on_other	do_( nop, "flush" )
				end
				in_( "th" ) bgn_
					on_( 'e' )	do_( nop, "the" )
					on_other	do_( nop, "flush" )
					end
					in_( "the" ) bgn_
						on_( 'n' )	do_( nop, "then" )
						on_other	do_( nop, "flush" )
						end
						in_( "then" ) bgn_
							on_( ' ' )	do_( nothing, "" )
							on_( '\t' )	do_( nothing, "" )
							on_( '\n' )	do_( nothing, "" )
							on_other	do_( nop, "flush" )
							end
			in_( "." ) bgn_
				on_( '\n' )	do_( pop, "" ) state = "";
				on_other	do_( nop, "flush" )
				end
			in_( "flush" ) bgn_
				on_( '\n' )	do_( nop, base )
				on_other	do_( nop, same )
				end
		end
	}
	while ( strcmp( state, "" ) );
	stack->narrative.state.closure = 0;
	if ( event < 0 )
		;
	else if ( context->command.level < level ) {
		// did pop at last
		StackVA *stack = (StackVA *) context->command.stack->ptr;

		stack->narrative.state.event = 0;
		stack->narrative.state.condition = 0;
		stack->narrative.state.action = 1;
		stack->narrative.state.then = 0;

		*next_state = base;

		// now need to replace last instruction with "/."
		if ( context->record.instructions->next != NULL ) {
			char *string; asprintf( &string, "/.\n" );
			addItem( &context->record.instructions, string );
			listItem *next_i = context->record.instructions->next;
			context->record.instructions->next = next_i->next;
			free( next_i->ptr );
			freeItem( next_i );
		}
		else {
			output( Warning, "no action specified - will be removed from narrative..." );
			freeListItem( &context->record.instructions );
		}
	} else {
		StackVA *stack = (StackVA *) context->command.stack->ptr;

		stack->narrative.state.event = 0;
		stack->narrative.state.condition = 0;
		stack->narrative.state.action = 0;
		stack->narrative.state.then = 1;

		*next_state = "then";
	}
	return event;
}

static int
push_narrative_action( char *state, int event, char **next_state, _context *context )
{
	int retval = 0;
	event = push( state, event, next_state, context );
	*next_state = base;

	struct { int mode, level; } backup;
	backup.mode = context->command.mode;
	backup.level = context->command.level;

	set_command_mode( InstructionMode, context );
	set_record_mode( RecordInstructionMode, event, context );
	event = command_read( NULL, InstructionBlock, base, 0, context );
	set_record_mode( OffRecordMode, event, context );
	set_command_mode( backup.mode, context );

	if ( event < 0 )
		;
	else if ( context->command.level == backup.level ) {
		// returned from '/' - did not pop yet
		event = read_action_closure( base, event, next_state, context );
	}

	if ( event < 0 )
		;
	else if ( context->command.level < backup.level ) {
		// returned from '/.' - did pop already
		// set this stack level to action
		reorderListItem( &context->record.instructions );
		retval = set_narrative_action( state, '\n', next_state, context );
		StackVA *stack = context->command.stack->ptr;
		stack->narrative.thread = NULL;
	}
	else {
		// returned from '/' - did not pop
		// set the previous stack level to action
		reorderListItem( &context->record.instructions );
		listItem *s = context->command.stack;
		context->command.stack = s->next;
		context->command.level--;
		retval = set_narrative_action( state, 0, next_state, context );
		context->command.level++;
		context->command.stack = s;
	}
	return ( retval < 0 ? retval : event );
}

/*---------------------------------------------------------------------------
	read_narrative_else
---------------------------------------------------------------------------*/
static int
read_narrative_else( char *state, int event, char **next_state, _context *context )
{
	do_( on_token, "else\0else" );
	bgn_
	in_( "else" ) do {
		event = read_input( state, 0, NULL, context );
		bgn_
			on_( ' ' )	do_( nothing, "" )
			on_( '\t' )	do_( nothing, "" )
			on_( '\n' )	do_( nothing, "" )
			on_other	do_( error, "" )
			end
		}
		while ( strcmp( state, "" ) );
	end

	return event;
}

/*---------------------------------------------------------------------------
	narrative_init
---------------------------------------------------------------------------*/
static int
narrative_init( char *state, int event, char **next_state, _context *context )
{
	context->narrative.mode.editing = 1;

	context->narrative.backup.identifier.ptr = context->identifier.id[ 0 ].ptr;
	context->identifier.id[ 0 ].ptr = NULL;
	context->narrative.backup.expression.results = context->expression.results;
	context->expression.results = NULL;

	Narrative *narrative = newNarrative();
	narrative->name = context->identifier.id[ 1 ].ptr;
	context->identifier.id[ 1 ].ptr = NULL;
	context->narrative.current = narrative;

	event = push( state, event, next_state, context );
	*next_state = base;

	context->narrative.level = context->command.level;
	return event;
}

/*---------------------------------------------------------------------------
	narrative_exit
---------------------------------------------------------------------------*/
static int
narrative_exit( char *state, int event, char **next_state, _context *context )
{
	Narrative *narrative = context->narrative.current;

	freeListItem( &context->expression.results );
	context->expression.results = context->narrative.backup.expression.results;
	context->narrative.backup.expression.results = NULL;

	free( context->identifier.id[ 0 ].ptr );
	context->identifier.id[ 0 ].ptr = context->narrative.backup.identifier.ptr;
	context->narrative.backup.identifier.ptr = NULL;

	free( context->identifier.id[ 1 ].ptr );
	context->identifier.id[ 1 ].ptr = NULL;

	if ( narrative != NULL )
	{
		reorderNarrative( &narrative->root );
		if ( is_narrative_dangling( &narrative->root, 0 ) || !narrative->root.sub.num )
		{
			free( narrative->name );
			free( narrative );
			context->narrative.current = NULL;
		}
		else {
			context->identifier.id[ 1 ].ptr = strdup( narrative->name );
		}
	}
	context->narrative.level = 0;
	context->narrative.mode.editing = 0;
	return 0;
}

/*---------------------------------------------------------------------------
	narrative_error
---------------------------------------------------------------------------*/
static int
narrative_error( char *state, int event, char **next_state, _context *context )
{
	if (( context->input.stack ))
	{
		free( context->record.string.ptr );
		context->record.string.ptr = NULL;
		freeInstructionBlock( context );

		while ( context->command.level >= context->narrative.level )
			pop( base, 0, &same, context );

		if ( context->input.stack != NULL )
			pop_input( base, 0, &same, context );

		freeNarrative( context->narrative.current );
		context->narrative.current = NULL;
		*next_state = "";
	}
	else
	{
		do_( flush_input, same );
		if ( !strncmp( state, "in (", 3 ) || !strncmp( state, "on (", 3 ) ) {
			Occurrence *thread = retrieve_narrative_thread( context );
			for ( listItem *i = thread->va; i!=NULL; i=i->next ) {
				freeOccurrence( i->ptr );
			}
			freeListItem( &thread->va );
			thread = thread->thread;
			if (( thread != NULL ) && ( thread->sub.num > 0 )) {
				free( thread->sub.n->ptr );
				popListItem( &thread->sub.n );
				thread->sub.num--;
			}
			StackVA *stack = (StackVA *) context->command.stack->ptr;
			stack->narrative.state.otherwise = 0;
			stack->narrative.thread = NULL;
		}
		*next_state = base;
	}
	return event;
}

/*---------------------------------------------------------------------------
	read_narrative
---------------------------------------------------------------------------*/
int
read_narrative( char *state, int event, char **next_state, _context *context )
{
	switch ( context->command.mode )
	{
	case FreezeMode:
		return 0;
	case InstructionMode:
		return output( Error, "narrative definition is only supported in Execution Mode" );
	case ExecutionMode:
		break;
	}

	do_( narrative_init, base );

	while( strcmp( state, "" ) )
	{
		event = read_input( state, event, NULL, context );
#ifdef DEBUG
		outputf( Debug, "narrative: in \"%s\", on '%c'", state, event );
#endif
		bgn_
		on_( 0 )	output( Error, "reached premature EOF - restoring original stack level" );
				do_( narrative_error, "" )
		on_( -1 )	do_( narrative_error, base )
		in_( base ) bgn_
			on_( ' ' )	do_( nop, same )
			on_( '\t' )	do_( nop, same )
			on_( '\n' )	do_( nop, same )
			on_( '/' )	do_( nop, "/" )
			on_( 'i' )	do_( read_narrative_in, "in" )
			on_( 'o' )	do_( read_narrative_on, "on" )
			on_( 'e' )	do_( read_narrative_else, "else" )
			on_( 'd' )	do_( read_narrative_do, "do" )
			on_( 't' )	do_( read_narrative_then, "then" )
			on_other	do_( error, same)
			end

		in_( "/" ) bgn_
			on_( ' ' )	do_( nop, same )
			on_( '\t' )	do_( nop, same )
			on_( '~' )	do_( nop, "/~" )
			on_( '\n' )	do_( nothing, pop_state )
			on_other	do_( error, base )
			end

		in_( "else" ) bgn_
			on_( ' ' )	do_( nop, same )
			on_( '\t' )	do_( nop, same )
			on_( '\n' )	do_( set_narrative_otherwise, same )
					do_( push_narrative_otherwise, base )
			on_( 'i' )	do_( set_narrative_otherwise, same )
					do_( read_narrative_in, "in" )
			on_( 'o' )	do_( set_narrative_otherwise, same )
					do_( read_narrative_on, "on" )
			on_( 'd' )	do_( set_narrative_otherwise, same )
					do_( read_narrative_do, "do" )
			on_other	do_( error, same )
			end

		in_( "in" ) bgn_
			on_( ' ' )	do_( nop, same )
			on_( '\t' )	do_( nop, same )
			on_( '(' )	do_( narrative_condition_begin, "in (" )
			on_other	do_( read_narrative_condition, "in_" )
			end
			in_( "in_" ) bgn_
				on_( ' ' )	do_( set_narrative_condition, "in (_)" )
				on_( '\t' )	do_( set_narrative_condition, "in (_)" )
				on_( '\n' )	do_( set_narrative_condition, same )
						do_( push_narrative_condition, base )
				on_( 'i' )	do_( set_narrative_condition, same )
						do_( read_narrative_in, "in" )
				on_( 'o' )	do_( set_narrative_condition, same )
						do_( read_narrative_on, "on" )
				on_( 'd' )	do_( set_narrative_condition, same )
						do_( read_narrative_do, "do" )
				on_other	do_( error, same )
				end
			in_( "in (" ) bgn_
				on_( ' ' )	do_( nop, same )
				on_( '\t' )	do_( nop, same )
				on_other	do_( read_narrative_condition, "in (_" )
				end
				in_( "in (_" ) bgn_
					on_( ' ' )	do_( nop, same )
					on_( '\t' )	do_( nop, same )
					on_( ',' )	do_( narrative_condition_add, "in (" )
					on_( ')' )	do_( narrative_condition_end, "in (_)" )
					on_other	do_( error, same )
					end
			in_( "in (_)" ) bgn_
				on_( ' ' )	do_( nop, same )
				on_( '\t' )	do_( nop, same )
				on_( '\n' )	do_( push_narrative_condition, base )
				on_( 'i' )	do_( read_narrative_in, "in" )
				on_( 'o' )	do_( read_narrative_on, "on" )
				on_( 'd' )	do_( read_narrative_do, "do" )
				on_other	do_( error, same )
				end

		in_( "on" ) bgn_
			on_( ' ' )	do_( nop, same )
			on_( '\t' )	do_( nop, same )
			on_( '(' )	do_( narrative_event_begin, "on (" )
			on_other	do_( read_narrative_event, "on_" )
			end
			in_( "on_" ) bgn_
				on_( ' ' )	do_( set_narrative_event, "on (_)" )
				on_( '\t' )	do_( set_narrative_event, "on (_)" )
				on_( '\n' )	do_( set_narrative_event, same )
						do_( push_narrative_event, base )
				on_( 'i' )	do_( set_narrative_event, same )
						do_( read_narrative_in, "in" )
				on_( 'o' )	do_( set_narrative_event, same )
						do_( read_narrative_on, "on" )
				on_( 'd' )	do_( set_narrative_event, same )
						do_( read_narrative_do, "do" )
				on_other	do_( error, same )
				end
			in_( "on (" ) bgn_
				on_( ' ' )	do_( nop, same )
				on_( '\t' )	do_( nop, same )
				on_other	do_( read_narrative_event, "on (_" )
				end
				in_( "on (_" ) bgn_
					on_( ' ' )	do_( nop, same )
					on_( '\t' )	do_( nop, same )
					on_( ',' )	do_( narrative_event_add, "on (" )
					on_( ')' )	do_( narrative_event_end, "on (_)" )
					on_other	do_( error, same )
					end
			in_( "on (_)" ) bgn_
				on_( ' ' )	do_( nop, same )
				on_( '\t' )	do_( nop, same )
				on_( '\n' )	do_( push_narrative_event, base )
				on_( 'i' )	do_( read_narrative_in, "in" )
				on_( 'o' )	do_( read_narrative_on, "on" )
				on_( 'd' )	do_( read_narrative_do, "do" )
				on_other	do_( error, same )
				end

		in_( "do" ) bgn_
			on_( ' ' )	do_( nop, same )
			on_( '\t' )	do_( nop, same )
			on_( '\n' )	do_( push_narrative_action, base )
			on_other	do_( read_narrative_action, "do_" )
			end
			in_( "do_" ) bgn_
				on_( ' ' )	do_( nop, same )
				on_( '\t' )	do_( nop, same )
				on_( '\n' )	do_( set_narrative_action, base )
				on_( 't' )	do_( set_narrative_action, same )
						do_( read_narrative_then, "then" )
				on_other	do_( error, same )
				end

		in_( "then" ) bgn_
			on_( ' ' )	do_( set_narrative_then, "then_" )
			on_( '\t' )	do_( set_narrative_then, "then_" )
			on_( '\n' )	do_( set_narrative_then, same )
					do_( push_narrative_then, base )
			on_other	do_( restore_narrative_action, base )
			end
			in_( "then_" ) bgn_
				on_( 'i' )	do_( read_narrative_in, "in" )
				on_( 'o' )	do_( read_narrative_on, "on" )
				on_( 'd' )	do_( read_narrative_do, "do" )
				on_other	do_( restore_narrative_action, base )
				end
		end
	}

	do_( narrative_exit, same );
	return event;
}

