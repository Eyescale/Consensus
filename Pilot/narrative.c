#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>

#include "list.h"
#include "registry.h"
#include "database.h"
#include "kernel.h"

#include "api.h"
#include "input.h"
#include "input_util.h"
#include "output.h"
#include "path.h"
#include "command.h"
#include "command_util.h"
#include "expression.h"
#include "expression_util.h"
#include "narrative.h"
#include "narrative_util.h"

// #define DEBUG

/*---------------------------------------------------------------------------
	narrative engine
---------------------------------------------------------------------------*/
#define do_( a, s )  \
	event = narrative_execute( a, &state, event, s, context );

static int
narrative_execute( _action action, char **state, int event, char *next_state, _context *context )
{
	if ( action == flush_input ) {
		StackVA *stack = (StackVA *) context->command.stack->ptr;
		stack->narrative.thread = NULL;
	}
	event = action( *state, event, &next_state, context );
        if ( strcmp( next_state, same ) ) *state = next_state;

	return event;
}

/*---------------------------------------------------------------------------
	narrative_pop
---------------------------------------------------------------------------*/
/*
   stack->narrative.state.whole		true if an action has been specified at a higher stack level
   stack->narrative.state.condition	true if a condition has been specified at this stack level
   stack->narrative.state.event		true if an event has been specified at this stack level
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

		if ( context->command.level < context->narrative.level ) {
			*next_state = "";
			return event;
		}
		stack = (StackVA *) context->command.stack->ptr;
		if ( stack->narrative.state.then ) {
			if ( !whole )
			/*
				if the user did not specify an action, and
				the stack state is then
			*/
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

/*---------------------------------------------------------------------------
	utility functions
---------------------------------------------------------------------------*/
static Occurrence *
current_occurrence( _context *context )
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

	occurrence->contrary = stack->narrative.state.contrary;
	stack->narrative.state.contrary = 0;

	// update narative stack's state whole in case of action
	if ( occurrence->type == ActionOccurrence )
	{
		if ( !stack->narrative.state.then )
		{
			listItem *s = context->command.stack;
			for ( int level = context->command.level; level>=context->narrative.level; level-- )
			{
				StackVA *sptr = (StackVA *) s->ptr;
				if ( sptr->narrative.state.whole ) break;
				sptr->narrative.state.whole = 1;
				s = s->next;
			}
		}
		stack->narrative.state.whole = 1;
		stack->narrative.state.otherwise = 0;
	}

	// retrieve and set parent occurrence - aka. narrative thread
	Occurrence *parent = current_occurrence( context );
	occurrence->thread = parent;

#ifdef DEBUG
	outputf( Debug, "building narrative: thread %0x", (int) parent );
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

		// update narrative thread
		if (( occurrence->type == ActionOccurrence ) && (event == '\n' )) {
			stack->narrative.thread = NULL;
		} else {
			stack->narrative.thread = occurrence;
		}
	}
	return 0;
}

/*---------------------------------------------------------------------------
	narrative_condition actions
---------------------------------------------------------------------------*/
static int
set_to_contrary( char *state, int event, char **next_state, _context *context )
{
	StackVA *stack = context->command.stack->ptr;
	stack->narrative.state.contrary = 1;
	return 0;
}

static int
read_narrative_condition( char *state, int event, char **next_state, _context *context )
{
	freeExpression( context->expression.ptr );
	context->expression.ptr = NULL;
	free_identifier( context, 0 );

	state = base;
	do {
		event = read_input( state, event, &same, context );
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
				on_other	do_( error, "" )
				end
			in_( "identifier:" ) bgn_
				on_any		do_( build_expression, "" )
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
	if ( test_identifier( context, 0 ) ) {
		condition->identifier = take_identifier( context, 0 );
	}
	return condition;
}

static int
add_narrative_condition( char *state, int event, char **next_state, _context *context )
{
	StackVA *stack = (StackVA *) context->command.stack->ptr;
	Occurrence *occurrence = current_occurrence( context );
	if ( stack->narrative.state.otherwise ) {
		occurrence = occurrence->va->ptr;
	}
	set_condition_value( occurrence, context );
	return 0;
}

static int
narrative_condition_end( char *state, int event, char **next_state, _context *context )
{
	StackVA *stack = (StackVA *) context->command.stack->ptr;
	Occurrence *occurrence = current_occurrence( context );
	if ( stack->narrative.state.otherwise ) {
		occurrence = occurrence->va->ptr;
	}
	set_condition_value( occurrence, context );
	reorderListItem( &occurrence->va );
	stack->narrative.state.otherwise = 0;
	return 0;
}

static int
set_narrative_condition( char *state, int event, char **next_state, _context *context )
{
	if ( event < 0 ) return event;
	Occurrence *occurrence = newOccurrence( ConditionOccurrence );
	set_condition_value( occurrence, context );
	int retval = narrative_build( occurrence, event, context );
	return ( retval < 0 ) ? retval : event;
}

static int
push_narrative_condition( char *state, int event, char **next_state, _context *context )
{
	StackVA *stack = (StackVA *) context->command.stack->ptr;
	if ( !stack->narrative.state.then ) {
		stack->narrative.state.condition = 1;
	}
	event = push( state, event, next_state, context );
	*next_state = base;
	return event;
}

/*---------------------------------------------------------------------------
	narrative_event actions
---------------------------------------------------------------------------*/
/*
	check against 'on init'-on or on-in-on sequence
*/
static int
check_narrative_on_( char *state, int event, char **next_state, _context *context )
{
	StackVA *stack = (StackVA *) context->command.stack->ptr;
	Occurrence *parent = stack->narrative.thread;
	if ( parent == NULL ) {
		if ( context->command.level == context->narrative.level ) {
			return 0;	// no problem
		} else {
			StackVA *s = (StackVA *) context->command.stack->next->ptr;
			parent = s->narrative.thread;
		}
	}
	for ( ; parent != NULL; parent=parent->thread ) {
		if ( parent->type == ThenOccurrence )
			break;
		if ( parent->type == EventOccurrence ) {
			EventVA *e = (EventVA *) parent->va->ptr;
			if ( e->type == InitEvent ) {
				return output( Error, "narrative 'on init'-on sequence is not supported" );
			}
		}
	}
	parent = stack->narrative.thread;
	if ( parent == NULL ) {
		StackVA *s = (StackVA *) context->command.stack->next->ptr;
		parent = s->narrative.thread;
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
	return 0;
}

static int
check_init_event( char *state, int event, char **next_state, _context *context )
{
	if ( !strcmp( get_identifier( context, 0 ), "init" ) ) {
		if ( context->command.level > context->narrative.level ) {
			return output( Error, "narrative on-init sequence must start from base" );
		}
		StackVA *stack = (StackVA *) context->command.stack->ptr;
		stack->narrative.event.type = InitEvent;
		return event;
	}
	return outputf( Error, "on %s%c*****trailing characters", get_identifier( context, 0 ), event );
}

static int
set_event_identifier( char *state, int event, char **next_state, _context *context )
{
	if ( !strcmp( get_identifier( context, 0 ), "init" ) ) {
		return output( Error, "on init*****trailing characters" );
	}
	StackVA *stack = (StackVA *) context->command.stack->ptr;
	stack->narrative.event.identifier = take_identifier( context, 0 );
	return 0;
}

static int
set_event_scheme( char *state, int event, char **next_state, _context *context )
{
	StackVA *stack = (StackVA *) context->command.stack->ptr;
	char *scheme = get_identifier( context, 0 );
	if ( !strcmp( scheme, "cgi" ) )
	{
		if ( context->identifier.path != NULL ) {
			free( context->identifier.path );
			context->identifier.path = NULL;
			return outputf( Error, "on < cgi*****trailing characters" );
		}
		stack->narrative.event.source.scheme = CGIScheme;
	}
	else if ( !strcmp( scheme, "operator" ) )
	{
		if ( context->identifier.path != NULL ) {
			free( context->identifier.path );
			context->identifier.path = NULL;
			return outputf( Error, "on < operator*****trailing characters" );
		}
		stack->narrative.event.source.scheme = OperatorScheme;
	}
	else if ( !strcmp( scheme, "session" ) )
	{
		stack->narrative.event.source.scheme = SessionScheme;
		stack->narrative.event.source.path = context->identifier.path;
		context->identifier.path = NULL;
	}
	else if ( !strcmp( scheme, "file" ) )
	{
		stack->narrative.event.source.scheme = FileScheme;
		stack->narrative.event.source.path = context->identifier.path;
		context->identifier.path = NULL;
	}
	else
	{
		return outputf( Error, "on < '%s'*****unknown scheme", scheme );
	}
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
read_narrative_event( char *state, int event, char **next_state, _context *context )
{
	freeExpression( context->expression.ptr );
	context->expression.ptr = NULL;
	free( context->identifier.path );
	context->identifier.path = NULL;

	StackVA *stack = (StackVA *) context->command.stack->ptr;
	memset( &stack->narrative.event, 0, sizeof( EventVA ) );
	state = base;
	do {
		event = read_input( state, event, &same, context );
#ifdef DEBUG
		outputf( Debug, "read_narrative_event: in \"%s\", on '%c'", state, event );
#endif
		bgn_
		on_( 0 )	do_( error, "" )
		on_( -1 )	do_( error, "" )
		on_( ' ' )	do_( nop, same )
		on_( '\t' )	do_( nop, same )
		in_( base ) bgn_
			on_( '<' )	do_( nop, "identifier <" )
			on_( ':' )	do_( nop, "identifier:" )
			on_other	do_( read_0, "identifier" )
			end
			in_( "identifier" ) bgn_
				on_( ':' )	do_( set_event_identifier, "identifier:" )
				on_( '<' )	do_( set_event_identifier, "identifier <" )
				on_other	do_( check_init_event, "" )
				end
			in_( "identifier <" ) bgn_
				on_any	do_( read_0, "identifier < scheme" )
				end
				in_( "identifier < scheme" ) bgn_
					on_( '>' )	do_( set_event_scheme, "identifier <_>" )
					on_( ':' )	do_( read_path, "identifier < scheme://path" )
					on_other	do_( error, "" )
					end
				in_( "identifier < scheme://path" ) bgn_
					on_( '>' )	do_( set_event_scheme, "identifier <_>" )
					on_other	do_( error, "" )
					end
				in_( "identifier <_>" ) bgn_
					on_( ':' )	do_( nop, "identifier:" )
					on_other	do_( error, "" )
					end
			in_( "identifier:" ) bgn_
#ifndef DO_LATER
				on_any	do_( build_expression, "identifier: expression" )
				end
#else
				on_( '<' )	do_( nop, "identifier: <" )
				on_other	do_( build_expression, "identifier: expression" )
				end
				in_( "identifier: <" ) bgn_
					on_any	do_( read_scheme, "identifier: < scheme" )
					end
					in_( "identifier: < scheme" ) bgn_
						on_( '>' )	do_( nop, "identifier: <_>" )
						on_( ':' )	do_( nop, "identifier: < scheme:" )
						on_other	do_( error, "" )
						end
					in_( "identifier: < scheme:" ) bgn_
						on_any	do_( read_path, "identifier: < scheme://path" )
						end
					in_( "identifier: < scheme://path" ) bgn_
						on_( '>' )	do_( nop, "identifier: <_>" )
						on_other	do_( error, "" )
						end
					in_( "identifier: <_>" ) bgn_
						on_( '!' )	do_( nop, "identifier: <_> !" )
						end
						in_( "identifier: <_> !" ) bgn_
							on_( '!' )	do_( set_event_type, "" )
							on_( '~' )	do_( set_event_type, "" )
							on_( '*' )	do_( set_event_type, "" )
							on_( '_' )	do_( set_event_type, "" )
							on_( '<' )	do_( set_event_type, "" )
							on_other	do_( error, "" )
							end
#endif	// DO_LATER
				in_( "identifier: expression" ) bgn_
					on_( '!' )	do_( nop, "identifier: expression !" )
					on_other	do_( error, "" )
					end
					in_( "identifier: expression !" ) bgn_
						on_( '!' )	do_( set_event_type, "" )
						on_( '~' )	do_( set_event_type, "" )
						on_( '*' )	do_( set_event_type, "" )
						on_( '_' )	do_( set_event_type, "" )
						on_other	do_( error, "" )
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
		event->format = take_identifier( context, 1 );
	}
	else
	{
		event->format = context->expression.ptr;
		context->expression.ptr = NULL;
	}
	return event;
}

static int
add_narrative_event( char *state, int event, char **next_state, _context *context )
{
	StackVA *stack = (StackVA *) context->command.stack->ptr;
	if ( stack->narrative.event.type == InitEvent ) {
		return output( Error, "Usage: 'on init' - parentheses not supported" );
	}
	Occurrence *occurrence = current_occurrence( context );
	if ( stack->narrative.state.otherwise ) {
		occurrence = occurrence->va->ptr;
	}
	set_event_value( occurrence, &stack->narrative.event, context );
	return 0;
}

static int
narrative_event_end( char *state, int event, char **next_state, _context *context )
{
	StackVA *stack = (StackVA *) context->command.stack->ptr;
	event = add_narrative_event( state, event, next_state, context );
	Occurrence *occurrence = current_occurrence( context );
	if ( stack->narrative.state.otherwise ) {
		occurrence = occurrence->va->ptr;
	}
	reorderListItem( &occurrence->va );
	stack->narrative.state.otherwise = 0;
	return event;
}

static int
set_narrative_event( char *state, int event, char **next_state, _context *context )
{
	if ( event < 0 ) return event;

	StackVA *stack = (StackVA *) context->command.stack->ptr;
	Occurrence *occurrence = newOccurrence( EventOccurrence );
	set_event_value( occurrence, &stack->narrative.event, context );

	int retval = narrative_build( occurrence, event, context );
	return ( retval < 0 ) ? retval : event;
}

static int
push_narrative_event( char *state, int event, char **next_state, _context *context )
{
	StackVA *stack = (StackVA *) context->command.stack->ptr;
	if ( !stack->narrative.state.then ) {
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
	Occurrence *parent = current_occurrence( context );
	Occurrence *last = (Occurrence *) parent->sub.n->ptr;
	switch ( last->type ) {
	case EventOccurrence:
	case ConditionOccurrence:
		break;
	case OtherwiseOccurrence:
		if ( last->va == NULL ) {
			goto ERROR;
		}
		OtherwiseVA *otherwise = (OtherwiseVA *) last->va->ptr;
		if ( otherwise->occurrence == NULL ) {
			goto ERROR;
		}
		break;
	default:
		goto ERROR;
	}

	Occurrence *occurrence = newOccurrence( OtherwiseOccurrence );
	int retval = narrative_build( occurrence, event, context );
	stack->narrative.state.otherwise = 1;
	return ( retval < 0 ) ? retval : event;
ERROR:
	return output( Error, "no narrative event or condition to alternate" );
}

/*---------------------------------------------------------------------------
	narrative_then actions
---------------------------------------------------------------------------*/
static int
then_override( char *state, int event, char **next_state, _context *context )
{
	if ( event < 0 ) return event;
	StackVA *stack = (StackVA *) context->command.stack->ptr;
	stack->narrative.state.event = 0;
	stack->narrative.state.condition = 0;
	stack->narrative.state.action = 0;
	stack->narrative.state.then = 1;
	return event;
}

static int
then_restore( char *state, int event, char **next_state, _context *context )
{
	do_( error, same );
	StackVA *stack = (StackVA *) context->command.stack->ptr;
	stack->narrative.state.action = 1;
	return -1;
}

static int
set_narrative_do_then( char *state, int event, char **next_state, _context *context )
{
	StackVA *stack = (StackVA *) context->command.stack->ptr;
	if ( stack->narrative.state.exit ) {
		output( Warning, "'then' following exit - will be ignored" );
	}
	// do not set current stack->narrative.state.then
	Occurrence *occurrence = newOccurrence( ThenOccurrence );
	int retval = narrative_build( occurrence, event, context );

	return ( retval < 0 ) ? retval : event;
}

static int
set_narrative_then( char *state, int event, char **next_state, _context *context )
{
	StackVA *stack = (StackVA *) context->command.stack->ptr;
	if ( !stack->narrative.state.action && !stack->narrative.state.whole ) {
		return output( Error, "'then' must follow action..." );
	}
	if ( stack->narrative.state.otherwise ) {
		return outputf( Error, NULL );
	}
	if ( stack->narrative.state.exit ) {
		output( Warning, "'then' following exit - may be ignored" );
	}
	stack->narrative.state.then = 1;
	Occurrence *occurrence = newOccurrence( ThenOccurrence );
	int retval = narrative_build( occurrence, event, context );

	return ( retval < 0 ) ? retval : event;
}

static int
push_narrative_then( char *state, int event, char **next_state, _context *context )
{
	event = push( state, event, next_state, context );
	*next_state = base;
	return event;
}

/*---------------------------------------------------------------------------
	narrative_action actions
---------------------------------------------------------------------------*/
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
	outputf( Debug, "narrative: read action \"%s\" - returning '%c'", context->record.string.value, event );
#endif
	if ( event < 0 ) return error( state, event, next_state, context );

	// Note: context->record.instructions is assumed to be NULL here...
	addItem( &context->record.instructions, context->record.string.value );
	context->record.string.value = NULL;

	return event;
}

static ActionVA *set_action_value( Occurrence *, _context * );
static int
set_narrative_action( char *state, int event, char **next_state, _context *context )
{
	if ( event < 0 ) return event;

	Occurrence *occurrence = newOccurrence( ActionOccurrence );
	ActionVA *action = set_action_value( occurrence, context );
	int retval = narrative_build( occurrence, event, context );

	StackVA *stack = (StackVA *) context->command.stack->ptr;
	if ( action->exit ) {
		stack->narrative.state.exit = 1;
	}
	stack->narrative.state.otherwise = 0;
	stack->narrative.state.action = 1;
	return ( retval < 0 ) ? retval : event;
}

static ActionVA *
set_action_value( Occurrence *occurrence, _context *context )
{
	listItem *instructions = context->record.instructions;
	context->record.instructions = NULL;

	ActionVA *action = (ActionVA *) calloc( 1, sizeof(ActionVA) );
	if (( instructions )) {
		action->instructions = instructions;
		action->exit = !strcmp( instructions->ptr, "exit" );
	}
	addItem( &occurrence->va, action );
	occurrence->va_num++;
	return action;
}

static int
narrative_action_end( char *state, int event, char **next_state, _context *context )
{
	StackVA *stack = (StackVA *) context->command.stack->ptr;
	if ( stack->narrative.state.then )
		return narrative_pop( state, event, next_state, context );
	return 0;
}

static int
narrative_action_open( char *state, int event, char **next_state, _context *context )
{
	push( state, event, &base, context );

	struct { int mode, level; } backup;
	backup.mode = context->command.mode;
	backup.level = context->command.level;

	set_command_mode( InstructionMode, context );
	set_record_mode( RecordInstructionMode, event, context );
	event = command_read( NULL, InstructionBlock, base, 0, context );
	set_record_mode( OffRecordMode, event, context );
	set_command_mode( backup.mode, context );

	if ( event < 0 ) return error( state, event, next_state, context );

	if ( context->command.level < backup.level ) {
		// returned from '/. - did pop already
		reorderListItem( &context->record.instructions );
		int retval = set_narrative_action( state, '\n', next_state, context );
		if ( retval > 0 ) {
			StackVA *stack = context->command.stack->ptr;
			if ( stack->narrative.state.then )
				return narrative_pop( state, event, next_state, context );
		}
		return ( retval < 0 ) ? retval : '}';
	}

	// returned from '/' - did not pop (aka. close) yet
	StackVA *stack = (StackVA *) context->command.stack->ptr;
	stack->narrative.state.closure = 1; // just for prompt
	return 0;
}

static int
narrative_action_close( char *state, int event, char **next_state, _context *context )
{
	// pop at last
	pop( state, event, &same, context );

	// replace the last instruction (which was "/" ) with "/."
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
	reorderListItem( &context->record.instructions );

	// set this stack level to action
	int retval = set_narrative_action( state, event, next_state, context );
	if ( retval > 0 ) {
		StackVA *stack = context->command.stack->ptr;
		if ( stack->narrative.state.then )
			return narrative_pop( state, event, next_state, context );
	}
	return retval;
}

static int
narrative_action_then( char *state, int event, char **next_state, _context *context )
{
	StackVA *stack = (StackVA *) context->command.stack->ptr;
	stack->narrative.state.closure = 0;

	// set current stack level to then
	stack->narrative.state.condition = 0;
	stack->narrative.state.event = 0;
	stack->narrative.state.action = 0;
	stack->narrative.state.then = 1;

	// set the previous stack level to action
	reorderListItem( &context->record.instructions );
	listItem *s = context->command.stack;
	context->command.stack = s->next;
	context->command.level--;
	int retval = set_narrative_action( state, 0, next_state, context );
	context->command.level++;
	context->command.stack = s;

	if ( retval < 0 ) return retval;
	
	Occurrence *occurrence = newOccurrence( ThenOccurrence );
	retval = narrative_build( occurrence, event, context );

	return ( retval < 0 ) ? retval : event;
}

/*---------------------------------------------------------------------------
	narrative_init
---------------------------------------------------------------------------*/
static int
narrative_init( char *state, int event, char **next_state, _context *context )
{
	context->narrative.mode.editing = 1;

	context->narrative.backup.identifier = take_identifier( context, 0 );
	context->narrative.backup.expression.results = context->expression.results;
	context->expression.results = NULL;

	Narrative *narrative = newNarrative();
	narrative->name = take_identifier( context, 1 );
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

	free_identifier( context, 0 );
	set_identifier( context, 0, context->narrative.backup.identifier );
	context->narrative.backup.identifier = NULL;

	free_identifier( context, 1 );
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
			set_identifier( context, 1, strdup( narrative->name ) );
		}
	}
	context->narrative.level = 0;
	context->narrative.mode.editing = 0;
	return 0;
}

/*---------------------------------------------------------------------------
	narrative_err
---------------------------------------------------------------------------*/
static int
narrative_err( char *state, int event, char **next_state, _context *context )
{
	if (( context->input.stack ))	// abort script
	{
		string_start( &context->record.string, 0 );
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
		if ( !strncmp( state, "in (", 4 ) || !strncmp( state, "on (", 4 ) ) {
			Occurrence *occurrence = current_occurrence( context );
			for ( listItem *i = occurrence->va; i!=NULL; i=i->next ) {
				freeOccurrence( i->ptr );
			}
			freeListItem( &occurrence->va );
			occurrence = occurrence->thread;
			if (( occurrence != NULL ) && ( occurrence->sub.num > 0 )) {
				free( occurrence->sub.n->ptr );
				popListItem( &occurrence->sub.n );
				occurrence->sub.num--;
			}
			StackVA *stack = (StackVA *) context->command.stack->ptr;
			stack->narrative.state.otherwise = 0;
			stack->narrative.thread = NULL;
		}
		*next_state = base;
	}
	context->error.tell = 1;
	return event;
}

/*---------------------------------------------------------------------------
	read_narrative
---------------------------------------------------------------------------*/
int
read_narrative( char *state, int event, char **next_state, _context *context )
{
	switch ( context->command.mode ) {
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
		event = read_input( state, event, &same, context );
#ifdef DEBUG
		outputf( Debug, "narrative: in \"%s\", on '%c'", state, event );
#endif
		bgn_
		on_( 0 )	output( Error, "reached premature EOF - restoring original stack level" );
				do_( narrative_err, "" )
		on_( -1 )	do_( narrative_err, base )

		in_( base ) bgn_
			on_( ' ' )	do_( nop, same )
			on_( '\t' )	do_( nop, same )
			on_( '\n' )	do_( nop, same )
			on_( '/' )	do_( nop, "/" )
			on_( 'i' )	do_( nop, "i" )
			on_( 'o' )	do_( nop, "o" )
			on_( 'd' )	do_( nop, "d" )
			on_( 'e' )	do_( read_token, "else\0else" )
			on_( 't' )	do_( read_token, "then\0then" )
					do_( then_override, same )
			on_other	do_( error, base )
			end

		in_( "i" ) bgn_
			on_( 'n' ) 	do_( nop, "in" )
			on_other	do_( error, base )
			end
			in_( "in" ) bgn_
				on_( ' ' )	do_( nop, "in " )
				on_( '\t' )	do_( nop, "in " )
				on_( '(' )	do_( nothing, "in " )
				on_( '~' )	do_( set_to_contrary, "in " )
				on_other	do_( error, base )
				end
				in_( "in " ) bgn_
					on_( ' ' )	do_( nop, same )
					on_( '\t' )	do_( nop, same )
					on_( '~' )	do_( set_to_contrary, same )
					on_( '(' )	do_( narrative_condition_begin, "in (" )
					on_other	do_( read_narrative_condition, same )
							do_( set_narrative_condition, "in (_)" )
					end
					in_( "in (" ) bgn_
						on_( ' ' )	do_( nop, same )
						on_( '\t' )	do_( nop, same )
						on_other	do_( read_narrative_condition, "in (_" )
						end
						in_( "in (_" ) bgn_
							on_( ' ' )	do_( nop, same )
							on_( '\t' )	do_( nop, same )
							on_( ',' )	do_( add_narrative_condition, "in (" )
							on_( ')' )	do_( narrative_condition_end, "in (_)" )
							on_other	do_( error, base )
							end
					in_( "in (_)" ) bgn_
						on_( '\n' )	do_( push_narrative_condition, base )
						on_( ' ' )	do_( nop, same )
						on_( '\t' )	do_( nop, same )
						on_( 'i' )	do_( nop, "i" )
						on_( 'o' )	do_( nop, "o" )
						on_( 'd' )	do_( nop, "d" )
						on_other	do_( error, base )
						end

		in_( "o" ) bgn_
			on_( 'n' )	do_( check_narrative_on_, "on" )
			on_other	do_( error, base )
			end
			in_( "on" ) bgn_
				on_( ' ' )	do_( nop, "on " )
				on_( '\t' )	do_( nop, "on " )
				on_( '(' )	do_( nothing, "on " )
				on_other	do_( error, base )
				end
			in_( "on " ) bgn_
				on_( ' ' )	do_( nop, same )
				on_( '\t' )	do_( nop, same )
				on_( '(' )	do_( narrative_event_begin, "on (" )
				on_other	do_( read_narrative_event, same )
						do_( set_narrative_event, "on (_)" )
				end
				in_( "on (" ) bgn_
					on_( ' ' )	do_( nop, same )
					on_( '\t' )	do_( nop, same )
					on_other	do_( read_narrative_event, "on (_" )
					end
					in_( "on (_" ) bgn_
						on_( ' ' )	do_( nop, same )
						on_( '\t' )	do_( nop, same )
						on_( ',' )	do_( add_narrative_event, "on (" )
						on_( ')' )	do_( narrative_event_end, "on (_)" )
						on_other	do_( error, base )
						end
				in_( "on (_)" ) bgn_
					on_( '\n' )	do_( push_narrative_event, base )
					on_( ' ' )	do_( nop, same )
					on_( '\t' )	do_( nop, same )
					on_( 'i' )	do_( nop, "i" )
					on_( 'o' )	do_( nop, "o" )
					on_( 'd' )	do_( nop, "d" )
					on_other	do_( error, base )
					end

		in_( "d" ) bgn_
			on_( 'o' )	do_( nop, "do" )
			on_other	do_( error, base )
			end
			in_( "do" ) bgn_
				on_( ' ' )	do_( nop, "do " )
				on_( '\t' )	do_( nop, "do " )
				on_( '\n' )	do_( nothing, "do " )
				on_other	do_( error, base )
				end
				in_( "do " ) bgn_
					on_( ' ' )	do_( nop, same )
					on_( '\t' )	do_( nop, same )
					on_( '\n' )	do_( narrative_action_open, "do {" )
					on_other	do_( read_narrative_action, same )
							do_( set_narrative_action, "do_" )
					end
				in_( "do_" ) bgn_
					on_( ' ' )	do_( nop, same )
					on_( '\t' )	do_( nop, same )
					on_( '\n' )	do_( narrative_action_end, base )
					on_( 'e' )	do_( read_token, "else\0else" )
					on_( 't' )	do_( read_token, "then\0do_then" )
					on_other	do_( error, same )
					end
					in_( "do_then" ) bgn_
						on_( ' ' )	do_( set_narrative_do_then, "then_" )
						on_( '\t' )	do_( set_narrative_do_then, "then_" )
						on_( '\n' )	do_( set_narrative_do_then, "then_" )
						on_other	do_( then_restore, base )
						end
				in_( "do {" ) bgn_
					on_( '}' )	do_( nop, base )
					on_( ' ' )	do_( nop, same )
					on_( '\t' )	do_( nop, same )
					on_( '\n' )	do_( nop, same )
					on_( '.' )	do_( nop, "do {_}" )
					on_( 't' )	do_( nop, "do {_} t" )
					on_other	do_( nop, "do {_" )
					end
					in_( "do {_" ) bgn_
						on_( '\n' )	do_( nop, "do {" )
						on_other	do_( nop, same )
						end
					in_( "do {_}" ) bgn_
						on_( ' ' )	do_( nop, same )
						on_( '\t' )	do_( nop, same )
						on_( '\n' )	do_( narrative_action_close, base )
						on_other	do_( nop, "do {_" )
						end
					in_( "do {_} t" ) bgn_
						on_( 'h' )	do_( nop, "do {_} th" )
						on_other	do_( nop, "do {_" )
						end
						in_( "do {_} th" ) bgn_
							on_( 'e' )	do_( nop, "do {_} the" )
							on_other	do_( nop, "do {_" )
							end
						in_( "do {_} the" ) bgn_
							on_( 'n' )	do_( nop, "do {_} then" )
							on_other	do_( nop, "do {_" )
							end
						in_( "do {_} then" ) bgn_
							on_( ' ' )	do_( narrative_action_then, "then_" )
							on_( '\t' )	do_( narrative_action_then, "then_" )
							on_( '\n' )	do_( narrative_action_then, "then_" )
							on_other	do_( nop, "do {_" )
							end

		in_( "else" ) bgn_
			on_( ' ' )	do_( set_narrative_otherwise, "else_" )
			on_( '\t' )	do_( set_narrative_otherwise, "else_" )
			on_( '\n' )	do_( set_narrative_otherwise, "else_" )
			on_other	do_( nop, "else_" )
			end
			in_( "else_" ) bgn_
				on_( '\n' )	do_( push_narrative_otherwise, base )
				on_( ' ' )	do_( nop, same )
				on_( '\t' )	do_( nop, same )
				on_( 'i' )	do_( nop, "i" )
				on_( 'o' )	do_( nop, "o" )
				on_( 'd' )	do_( nop, "d" )
				on_other	do_( error, same )
				end

		in_( "then" ) bgn_
			on_( ' ' )	do_( set_narrative_then, "then_" )
			on_( '\t' )	do_( set_narrative_then, "then_" )
			on_( '\n' )	do_( set_narrative_then, "then_" )
			on_other	do_( then_restore, base )
			end
			in_( "then_" ) bgn_
				on_( '\n' )	do_( push_narrative_then, base )
				on_( ' ' )	do_( nop, same )
				on_( '\t' )	do_( nop, same )
				on_( 'i' )	do_( nop, "i" )
				on_( 'o' )	do_( nop, "o" )
				on_( 'd' )	do_( nop, "d" )
				on_other	do_( then_restore, base )
				end

		in_( "/" ) bgn_
			on_( ' ' )	do_( nop, same )
			on_( '\t' )	do_( nop, same )
			on_( '\n' )	do_( narrative_pop, pop_state )
			on_other	do_( error, base )
			end

		end
	}

	do_( narrative_exit, same );
	return event;
}

