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
#include "narrative_util.h"

// #define DEBUG
// #define DEBUG2

/*---------------------------------------------------------------------------
	narrative engine
---------------------------------------------------------------------------*/
#define narrative_do_( a, s )  \
	event = narrative_execute( a, &state, event, s, context );

static int
narrative_execute( _action action, char **state, int event, char *next_state, _context *context )
{
	if ( !strcmp( next_state, pop_state ) )
	{
		int pop_level = context->control.level;
		StackVA *stack = (StackVA *) context->control.stack->ptr;
		if ( !stack->narrative.state.whole && ( context->narrative.level < pop_level )) {
			fprintf( stderr, "consensus> Warning: last narrative statement incomplete - will be truncated or removed...\n" );
		}
		listItem *s = context->control.stack;
		for ( s=s->next, pop_level--; pop_level >= context->narrative.level; s=s->next, pop_level-- )
		{
			StackVA *next = (StackVA *) s->ptr;
			if ( next->narrative.state.then && !stack->narrative.state.whole ) {
				next->narrative.state.then = 0;
				next->narrative.state.action = 1;
				next->narrative.thread = NULL;
				if ( !next->narrative.state.whole )
					continue;
				break;
			}
			else if ( next->narrative.state.then && !next->narrative.state.action )
				continue;
			else if ( next->narrative.state.event || next->narrative.state.condition || next->narrative.state.action ) {
				next->narrative.thread = NULL;
				break;
			}
		}
		do { event = pop( *state, event, &next_state, context );
		} while ( context->control.level > pop_level );

		if ( pop_level < context->narrative.level ) {
			*state = "";
		} else {
			*state = next_state;
		}
	}
	else
	{
		if ( action == flush_input ) {
			StackVA *stack = (StackVA *) context->control.stack->ptr;
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
		fprintf( stderr, "debug> narrative: ( %d, %d, %d, %d )-in->( 1, %d, %d, %d )\n",
		stack->narrative.state.condition, stack->narrative.state.event, stack->narrative.state.action, stack->narrative.state.then,
		stack->narrative.state.condition, stack->narrative.state.action, stack->narrative.state.then );
	} else if ( from == push_narrative_event ) {
		fprintf( stderr, "debug> narrative: ( %d, %d, %d, %d )-on->( %d, 1, %d, %d )\n",
		stack->narrative.state.condition, stack->narrative.state.event, stack->narrative.state.action, stack->narrative.state.then,
		stack->narrative.state.event, stack->narrative.state.action, stack->narrative.state.then );
	} else if ( from == read_narrative_then ) {
		fprintf( stderr, "debug> narrative: ( %d, %d, %d, %d )-then->( 0, 0, 0, 1 )\n",
		stack->narrative.state.condition, stack->narrative.state.event, stack->narrative.state.action, stack->narrative.state.then );
		break;
	} else if ( from == push_narrative_then ) {
		fprintf( stderr, "debug> narrative: pushing ( %d, %d, %d, %d )-then->( %d, %d, %d, 1 )\n",
		stack->narrative.state.condition, stack->narrative.state.event, stack->narrative.state.action, stack->narrative.state.then,
		stack->narrative.state.condition, stack->narrative.state.event, stack->narrative.state.action );
	} else if ( from == set_narrative_action ) {
		fprintf( stderr, "debug> narrative: ( %d, %d, %d, %d )-do->( %d, %d, 1, %d )\n",
		stack->narrative.state.condition, stack->narrative.state.event, stack->narrative.state.action, stack->narrative.state.then,
		stack->narrative.state.condition, stack->narrative.state.event, stack->narrative.state.then );
	}
}
#endif

/*---------------------------------------------------------------------------
	utility functions
---------------------------------------------------------------------------*/
static int
narrative_build( Occurrence *occurrence, int event, _context *context )
{
	StackVA *stack = (StackVA *) context->control.stack->ptr;
	Occurrence *thread = stack->narrative.thread;
	if ( thread == NULL ) {
		if ( context->control.level == context->narrative.level ) {
			thread = &context->narrative.current->root;
		}
		else {
			thread = ((StackVA *) context->control.stack->next->ptr)->narrative.thread;
		}
	}
	occurrence->thread = thread;
#ifdef DEBUG
	fprintf( stderr, "debug> building narrative: thread %0x\n", (int) thread );
#endif
	addItem( &thread->sub.n, occurrence );
	thread->sub.num++;
	if (( occurrence->type == ActionOccurrence ) && (event == '\n' )) {
		stack->narrative.thread = NULL;
	} else {
		stack->narrative.thread = occurrence;
	}
	return 0;
}

static int
narrative_on_in_on_( int event, _context *context )
{
	StackVA *stack = (StackVA *) context->control.stack->ptr;
	Occurrence *parent = stack->narrative.thread;
	if ( parent == NULL ) {
		if ( context->control.level == context->narrative.level ) {
			return event;	// no problem
		} else {
			parent = ((StackVA *) context->control.stack->next->ptr)->narrative.thread;
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
			return log_error( context, event, "narrative on-in-on sequence is not supported" );
		}
	}
	return event;
}

static int
narrative_on_init_on_( int event, _context *context )
{
	StackVA *stack = (StackVA *) context->control.stack->ptr;
	Occurrence *parent = stack->narrative.thread;
	if ( parent == NULL ) {
		if ( context->control.level == context->narrative.level ) {
			return event;	// no problem
		} else {
			parent = ((StackVA *) context->control.stack->next->ptr)->narrative.thread;
		}
	}
	for ( ; parent != NULL; parent=parent->thread ) {
		if ( parent->type == ThenOccurrence )
			break;
		if (( parent->type == EventOccurrence ) && parent->va.event.type.init ) {
			return log_error( context, event, "narrative on-init-on sequence is not supported" );
		}
	}
	return event;
}

static int
narrative_then_on_init_( int event, _context *context )
{
	StackVA *stack = (StackVA *) context->control.stack->ptr;
	Occurrence *parent = stack->narrative.thread;
	if ( parent == NULL ) {
		if ( context->control.level == context->narrative.level ) {
			return event;	// no problem
		} else {
			parent = ((StackVA *) context->control.stack->next->ptr)->narrative.thread;
		}
	}
	for ( ; parent != NULL; parent=parent->thread ) {
		if ( parent->type == ThenOccurrence ) {
			return log_error( context, event, "narrative then-on-init sequence is not supported" );
		}
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
		event = input( state, 0, NULL, context );
		bgn_
		in_( "i" ) bgn_
			on_( 'n' )	narrative_do_( nop, "in" )
			on_other	narrative_do_( error, "" )
			end
			in_( "in" ) bgn_
				on_( ' ' )	narrative_do_( nothing, "" )
				on_( '\t' )	narrative_do_( nothing, "" )
				on_( '(' )	narrative_do_( nothing, "" )
				on_other	narrative_do_( error, "" )
				end
		end
	}
	while ( strcmp( state, "" ) );

	return event;
}
static int
read_narrative_condition( char *state, int event, char **next_state, _context *context )
/*
	condition can be any expression, that is:
		in ( expression )
	with support for such special cases as
		in ( %variable : ~. )		// variable null
		in ( %variable : ? )		// variable not null
*/
{
	freeExpression( context->expression.ptr );
	context->expression.ptr = NULL;
	context->narrative.mode.condition = 1;
	event = read_expression( state, event, next_state, context );
	context->narrative.mode.condition = 0;
	return event;
}

static int
set_narrative_condition( char *state, int event, char **next_state, _context *context )
{
	Occurrence *occurrence = (Occurrence *) calloc( 1, sizeof(Occurrence) );
	occurrence->type = ConditionOccurrence;
	occurrence->va.condition.expression = context->expression.ptr;
	context->expression.ptr = NULL;
	return narrative_build( occurrence, event, context );
}

static int
push_narrative_condition( char *state, int event, char **next_state, _context *context )
{
	StackVA *stack = (StackVA *) context->control.stack->ptr;
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
		event = input( state, 0, NULL, context );
		bgn_
		in_( "o" ) bgn_
			on_( 'n' )	narrative_do_( nop, "on" )
			on_other	narrative_do_( error, "" )
			end
			in_( "on" ) bgn_
				on_( ' ' )	narrative_do_( nothing, "" )
				on_( '\t' )	narrative_do_( nothing, "" )
				on_( '(' )	narrative_do_( nothing, "" )
				on_other	narrative_do_( error, "" )
				end
		end
	}
	while ( strcmp( state, "" ) );

	if  ( event != -1 ) {
		event = narrative_on_in_on_( event, context );
	}
	if ( event != -1 ) {
		event = narrative_on_init_on_( event, context );
	}
	return event;
}

static int
set_event_identifier( char *state, int event, char **next_state, _context *context )
{
	if ( !strcmp( context->identifier.id[ 0 ].ptr, "init" ) ) {
		return log_error( context, event, "'init' used as event identifier" );
	}
	StackVA *stack = (StackVA *) context->control.stack->ptr;
	stack->narrative.event.identifier.name = context->identifier.id[ 0 ].ptr;
	context->identifier.id[ 0 ].ptr = NULL;
	return 0;
}

static int
set_event_variable( char *state, int event, char **next_state, _context *context )
{
	if ( !strcmp( context->identifier.id[ 0 ].ptr, "init" ) ) {
		return log_error( context, event, "'init' used as event identifier" );
	}
	StackVA *stack = (StackVA *) context->control.stack->ptr;
	stack->narrative.event.identifier.type = VariableIdentifier;
	stack->narrative.event.identifier.name = context->identifier.id[ 0 ].ptr;
	context->identifier.id[ 0 ].ptr = NULL;
	return 0;
}

static int
set_event_narrative( char *state, int event, char **next_state, _context *context )
{
	StackVA *stack = (StackVA *) context->control.stack->ptr;
	stack->narrative.event.type.narrative = 1;
	return 0;
}

static int
set_event_request( char *state, int event, char **next_state, _context *context )
{
	StackVA *stack = (StackVA *) context->control.stack->ptr;
	stack->narrative.event.type.request = 1;
	return 0;
}

static int
set_event_notification( char *state, int event, char **next_state, _context *context )
{
	StackVA *stack = (StackVA *) context->control.stack->ptr;
	stack->narrative.event.type.notification = 1;
	return 0;
}

static int
set_event_type( char *state, int event, char **next_state, _context *context )
{
	StackVA *stack = (StackVA *) context->control.stack->ptr;
	switch ( event ) {
	case '!': stack->narrative.event.type.instantiate = 1; break;
	case '~': stack->narrative.event.type.release = 1; break;
	case '*': stack->narrative.event.type.activate = 1; break;
	case '_': stack->narrative.event.type.deactivate = 1; break;
	}
	return 0;
}

static int
check_init_event( char *state, int event, char **next_state, _context *context )
{
	if ( !strcmp( context->identifier.id[ 0 ].ptr, "init" ) ) {
		event = narrative_then_on_init_( event, context );
		if ( event != -1 ) {
			StackVA *stack = (StackVA *) context->control.stack->ptr;
			stack->narrative.event.type.init = 1;
		}
	}
	else {
		char *msg; asprintf( &msg, "unspecified event for identifier '%s'", context->identifier.id[ 0 ].ptr );
		event = log_error( context, event, msg ); free( msg );
	}
	return event;
}

static int
read_narrative_event( char *state, int event, char **next_state, _context *context )
/*
	event can be either
		1. on init
		2.1. on identifier : { !!, !~, !*, !_ } expression
		2.2. on identifier : expression { !!, !~, !*, !_ }
		3.1. on %identifier : { !!, !~, !*, !_ } expression
		3.2. on %identifier : expression { !!, !~, !*, !_ }
			where identifier has been declared as
			do : identifier < %connection
		4. on %identifier :: expression
			where identifier has been declared as either
			3.1. do : identifier < stream : expression
				   do : event <
				   then on %event: '\n'
			3.2. do : identifier : value
				   do : event : '\n'
				   on %event: '\n'
*/
{
	freeExpression( context->expression.ptr );
	context->expression.ptr = NULL;
	context->narrative.mode.event = 1;

	StackVA *stack = (StackVA *) context->control.stack->ptr;
	bzero( &stack->narrative.event, sizeof( EventVA ) );
	state = base;
	do
	{
		event = input( state, event, NULL, context );
#ifdef DEBUG
		fprintf( stderr, "debug> read_narrative_event: in \"%s\", on '%c'\n", state, event );
#endif
		bgn_
		on_( -1 )	narrative_do_( nothing, "" )
		in_( base ) bgn_
			on_( ':' )	narrative_do_( nop, "identifier:" )
			on_( '!' )	narrative_do_( set_event_request, "identifier: !" )
			on_( '%' )	narrative_do_( nop, "%" )
			on_other	narrative_do_( read_identifier, "identifier" )
			end
			in_( "%" ) bgn_
				on_any	narrative_do_( read_identifier, "%identifier" )
				end
				in_( "%identifier" ) bgn_
					on_( ':' )	narrative_do_( set_event_variable, "identifier:" )
					on_( ' ' )	narrative_do_( set_event_variable, "identifier" )
					on_( '\t' )	narrative_do_( set_event_variable, "identifier" )
					on_other	narrative_do_( error, base )
					end
			in_( "identifier" ) bgn_
				on_( ' ' )	narrative_do_( nop, same )
				on_( '\t' )	narrative_do_( nop, same )
				on_( ':' )	narrative_do_( set_event_identifier, "identifier:" )
				on_other	narrative_do_( check_init_event, "" )
				end
				in_( "identifier:" ) bgn_
					on_( ' ' )	narrative_do_( nop, same )
					on_( '\t' )	narrative_do_( nop, same )
					on_( '!' )	narrative_do_( set_event_request, "identifier: !" )
					on_other	narrative_do_( read_expression, "identifier: expression" )
					end
					in_( "identifier: !" )	bgn_
						on_( '!' )	narrative_do_( set_event_type, "identifier: !." )
						on_( '~' )	narrative_do_( set_event_type, "identifier: !." )
						on_( '*' )	narrative_do_( set_event_type, "identifier: !." )
						on_( '_' )	narrative_do_( set_event_type, "identifier: !." )
						on_other	narrative_do_( error, base )
						end
						in_( "identifier: !." ) bgn_
							on_any	narrative_do_( read_expression, "identifier: !. expression" )
							end
							in_( "identifier: !. expression" ) bgn_
								on_( '(' )	narrative_do_( set_event_narrative, "identifier: !. narrative(" )
								on_other	narrative_do_( nothing, "" )
								end
							in_( "identifier: !. narrative(" ) bgn_
								on_( ' ' )	narrative_do_( nop, same )
								on_( '\t' )	narrative_do_( nop, same )
								on_( ')' )	narrative_do_( nop, "" )
								on_other	narrative_do_( error, base )
								end
					in_( "identifier: expression" ) bgn_
						on_( ' ' )	narrative_do_( nop, same )
						on_( '\t' )	narrative_do_( nop, same )
						on_( '(' )	narrative_do_( nop, "identifier: narrative(" )
						on_( '!' )	narrative_do_( set_event_notification, "identifier: expression !" )
						on_other	narrative_do_( error, base )
						end
					in_( "identifier: narrative(" ) bgn_
						on_( ' ' )	narrative_do_( nop, same )
						on_( '\t' )	narrative_do_( nop, same )
						on_( ')' )	narrative_do_( nop, "identifier: narrative(_)" )
						on_other	narrative_do_( error, base )
						end
						in_( "identifier: narrative(_)" ) bgn_
							on_( ' ' )	narrative_do_( nop, same )
							on_( '\t' )	narrative_do_( nop, same )
							on_( '!' )	narrative_do_( set_event_notification, "identifier: expression !" )
							on_other	narrative_do_( error, base )
							end
					in_( "identifier: expression !" ) bgn_
						on_( '!' )	narrative_do_( set_event_type, "" )
						on_( '~' )	narrative_do_( set_event_type, "" )
						on_( '*' )	narrative_do_( set_event_type, "" )
						on_( '_' )	narrative_do_( set_event_type, "" )
						on_other	narrative_do_( error, base )
						end
		end
	}
	while ( strcmp( state, "" ) );

	context->narrative.mode.event = 0;
	return event;
}

static int
set_narrative_event( char *state, int event, char **next_state, _context *context )
{
	StackVA *stack = (StackVA *) context->control.stack->ptr;
#ifdef DEBUG
	fprintf( stderr, "debug> set_narrative_event:\n"
	"\tidentifier: %s, type: %d\n"
	"\tinstantiate: %d, release: %d, activate: %d, deactivate: %d, notification: %d, request: %d, init: %d\n",
	stack->narrative.event.identifier.name, stack->narrative.event.identifier.type,
	stack->narrative.event.type.instantiate, stack->narrative.event.type.release, stack->narrative.event.type.activate,
	stack->narrative.event.type.deactivate, stack->narrative.event.type.notification, stack->narrative.event.type.request,
	stack->narrative.event.type.init );
#endif
	Occurrence *occurrence = (Occurrence *) calloc( 1, sizeof(Occurrence) );
	occurrence->type = EventOccurrence;
	bcopy( &stack->narrative.event, &occurrence->va.event, sizeof( EventVA ) );

	if ( stack->narrative.event.type.narrative ) {
		occurrence->va.event.narrative_identifier = context->identifier.id[ 1 ].ptr;
		context->identifier.id[ 1 ].ptr = NULL;
	} else {
		occurrence->va.event.expression = context->expression.ptr;
		context->expression.ptr = NULL;
	}
	return narrative_build( occurrence, event, context );
}

static int
push_narrative_event( char *state, int event, char **next_state, _context *context )
{
	StackVA *stack = (StackVA *) context->control.stack->ptr;
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
	narrative_then actions
---------------------------------------------------------------------------*/
static int
read_narrative_then( char *state, int event, char **next_state, _context *context )
{
	StackVA *stack = (StackVA *) context->control.stack->ptr;
	if ( !stack->narrative.state.action && !stack->narrative.state.whole ) {
		return log_error( context, event, "'then' must follow action..." );
	}
	if ( stack->narrative.state.exit ) {
		fprintf( stderr, "consensus> Warning: 'then' following exit - may be ignored\n" );
	}
	int read_from_base = !strcmp( state, base );
	state = "t";
	do {
		event = input( state, 0, NULL, context );
		bgn_
		in_( "t" ) bgn_
			on_( 'h' )	narrative_do_( nop, "th" )
			on_other	narrative_do_( error, "" )
			end
			in_( "th" ) bgn_
				on_( 'e' )	narrative_do_( nop, "the" )
				on_other	narrative_do_( error, "" )
				end
				in_( "the" ) bgn_
					on_( 'n' )	narrative_do_( nop, "then" )
					on_other	narrative_do_( error, "" )
					end
					in_( "then" ) bgn_
						on_( ' ' )	narrative_do_( nothing, "" )
						on_( '\t' )	narrative_do_( nothing, "" )
						on_( '\n' )	narrative_do_( nothing, "" )
						on_other	narrative_do_( error, "" )
						end
		end
	}
	while ( strcmp( state, "" ) );

	if ( read_from_base && ( event != -1 ) )
	{
		StackVA *stack = (StackVA *) context->control.stack->ptr;
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
	narrative_do_( error, same );
	StackVA *stack = (StackVA *) context->control.stack->ptr;
	stack->narrative.state.action = 1;
	return -1;
}

static int
set_narrative_then( char *state, int event, char **next_state, _context *context )
{
	Occurrence *occurrence = (Occurrence *) calloc( 1, sizeof(Occurrence) );
	occurrence->type = ThenOccurrence;
	return narrative_build( occurrence, event, context );
}

static int
push_narrative_then( char *state, int event, char **next_state, _context *context )
{
	StackVA *stack = (StackVA *) context->control.stack->ptr;
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
		event = input( state, 0, NULL, context );
		bgn_
		in_( "d" ) bgn_
			on_( 'o' )	narrative_do_( nop, "do" )
			on_other	narrative_do_( error, "" )
			end
			in_( "do" ) bgn_
				on_( ' ' )	narrative_do_( nothing, "" )
				on_( '\t' )	narrative_do_( nothing, "" )
				on_( '\n' )	narrative_do_( nothing, "" )
				on_other	narrative_do_( error, "" )
				end
		end
	}
	while ( strcmp( state, "" ) );

	return event;
}

static int
read_narrative_action( char *state, int event, char **next_state, _context *context )
{
	context->control.mode = InstructionMode;
	set_input_mode( OnRecordMode, event, context );
	context->narrative.mode.action.one = 1;
	event = read_command( base, event, &same, context );
	context->narrative.mode.action.one = 0;
	set_input_mode( OffRecordMode, event, context );
	context->control.mode = ExecutionMode;

#ifdef DEBUG
	fprintf( stderr, "debug> narrative: read action \"%s\" - returning '%c'\n", context->record.string.ptr, event );
#endif
	if ( event >= 0 ) {
		// Note: context->record.instructions is assumed to be NULL here...
		addItem( &context->record.instructions, context->record.string.ptr );
		context->record.string.ptr = NULL;
	}

	return event;
}

static int
set_narrative_action( char *state, int event, char **next_state, _context *context )
{
	StackVA *stack = (StackVA *) context->control.stack->ptr;
	if ( !stack->narrative.state.action && !stack->narrative.state.then )
	{
#ifdef DEBUG2
		debug_stack( stack, set_narrative_action );
#endif
		stack->narrative.state.action = 1;
		int level = context->control.level;
		for ( listItem *s = context->control.stack; level>=context->narrative.level; s=s->next, level-- )
		{
			StackVA *stack = (StackVA *) s->ptr;
			if ( stack->narrative.state.whole ) break;
			stack->narrative.state.whole = 1;
		}
	}
	else stack->narrative.state.whole = 1;

	listItem *instructions = context->record.instructions;
	context->record.instructions = NULL;

	Occurrence *occurrence = (Occurrence *) calloc( 1, sizeof(Occurrence) );
	occurrence->type = ActionOccurrence;
	occurrence->va.action.instructions = instructions;
	int retval = narrative_build( occurrence, event, context );

	if (( instructions != NULL ) && !strcmp( instructions->ptr, "exit" )) {
		stack->narrative.state.exit = 1;
		occurrence->va.action.exit = 1;
	}
	if ( stack->narrative.state.then && ( event == '\n' ) ) {
		narrative_do_( nothing, pop_state );
		*next_state = state;
	}
	return retval;
}

static int
read_action_closure( char *state, int event, char **next_state, _context *context )
{
	int level = context->control.level;

	// set this stack level temporarily to action - until 'then'
	StackVA *stack = context->control.stack->ptr;
	stack->narrative.state.action = 1;

	do {
		if ( context->control.prompt ) {
			fprintf( stderr, "(then/.)?_ " );
			for ( int i=0; i<context->control.level; i++ )
				fprintf( stderr, "\t" );
			context->control.prompt = 0;
		}
		event = input( state, 0, NULL, context );
		bgn_
		on_( -1 )	narrative_do_( nothing, "" )
		in_( base ) bgn_
			on_( ' ' )	narrative_do_( nop, same )
			on_( '\t' )	narrative_do_( nop, same )
			on_( '\n' )	narrative_do_( nop, same )
			on_( '.' )	narrative_do_( nop, "." )
			on_( 't' )	narrative_do_( read_narrative_then, "" )
			on_other	narrative_do_( nop, "flush" )
			end
			in_( "." ) bgn_
				on_( '\n' )	narrative_do_( pop, "" )
						state = "";
				on_other	narrative_do_( nop, "flush" )
				end
			in_( "flush" ) bgn_
				on_( '\n' )	narrative_do_( nop, base )
				on_other	narrative_do_( nop, same )
				end
		end
	}
	while ( strcmp( state, "" ) );

	if ( event < 0 )
		;
	if ( context->control.level < level ) {
		*next_state = base;
		// did pop at last - now need to replace last instruction with "/."
		if ( context->record.instructions->next != NULL ) {
			char *string; asprintf( &string, "/.%c", '\n' );
			addItem( &context->record.instructions, string );
			listItem *next_i = context->record.instructions->next;
			context->record.instructions->next = next_i->next;
			free( next_i->ptr );
			freeItem( next_i );
		}
		else {
			fprintf( stderr, "consensus> Warning: no action specified - will be removed from narrative...\n" );
			freeListItem( &context->record.instructions );
		}
	} else {
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

	int level = context->control.level;

	context->narrative.mode.action.block = 1;
	set_control_mode( InstructionMode, event, context );
	event = read_command( base, 0, &same, context );
	set_control_mode( ExecutionMode, event, context );

	if ( event < 0 )
		;
	else if ( context->control.level == level ) {
		// returned from '/' - did not pop yet
		event = read_action_closure( base, event, next_state, context );
	}

	if ( event < 0 )
		;
	else if ( context->control.level < level ) {
		// returned from '/.' - did pop already
		// set this stack level to action
		reorderListItem( &context->record.instructions );
		retval = set_narrative_action( state, '\n', next_state, context );
		StackVA *stack = context->control.stack->ptr;
		stack->narrative.thread = NULL;
	}
	else {
		// returned from '/' - did not pop
		// set the previous stack level to action
		reorderListItem( &context->record.instructions );
		listItem *s = context->control.stack;
		context->control.stack = s->next;
		retval = set_narrative_action( state, 0, next_state, context );
		context->control.stack = s;
	}
	context->narrative.mode.action.block = 0;
	return ( retval < 0 ? retval : event );
}

/*---------------------------------------------------------------------------
	narrative_init
---------------------------------------------------------------------------*/
static int
narrative_init( char *state, int event, char **next_state, _context *context )
{
	context->narrative.mode.script = ( context->input.stack != NULL );

	context->narrative.backup.identifier.ptr = context->identifier.id[ 0 ].ptr;
	context->identifier.id[ 0 ].ptr = NULL;
	context->narrative.backup.expression.results = context->expression.results;
	context->expression.results = NULL;

	Narrative *narrative = (Narrative *) calloc( 1, sizeof( Narrative ) );
	narrative->name = context->identifier.id[ 1 ].ptr;
	context->identifier.id[ 1 ].ptr = NULL;
	context->narrative.current = narrative;

	event = push( state, event, next_state, context );
	*next_state = base;

	context->narrative.level = context->control.level;
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
		if ( is_narrative_dangling( &narrative->root, 0 ) )
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
	context->narrative.mode.script = 0;
	return 0;
}

/*---------------------------------------------------------------------------
	narrative_error
---------------------------------------------------------------------------*/
static int
narrative_error( char *state, int event, char **next_state, _context *context )
{
	if ( context->narrative.mode.script )
	{
		free( context->record.string.ptr );
		context->record.string.ptr = NULL;
		freeInstructionBlock( context );

		while ( context->control.level >= context->narrative.level )
			pop( base, 0, NULL, context );

		if ( context->input.stack != NULL )
			pop_input( base, 0, NULL, context );

		freeNarrative( context->narrative.current );
		context->narrative.current = NULL;
		*next_state = "";
	}
	else
	{
		narrative_do_( flush_input, same );
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
	switch ( context->control.mode )
	{
	case FreezeMode:
		return 0;
	case InstructionMode:
		return log_error( context, event, "narrative definition is only supported in Execution Mode" );
	case ExecutionMode:
		break;
	}

	narrative_do_( narrative_init, base );

	while( strcmp( state, "" ) )
	{
		event = input( state, event, NULL, context );
#ifdef DEBUG
		fprintf( stderr, "debug> narrative: in \"%s\", on '%c'\n", state, event );
#endif
		bgn_
		on_( -1 )	narrative_do_( narrative_error, base )
		in_( base ) bgn_
			on_( ' ' )	narrative_do_( nop, same )
			on_( '\t' )	narrative_do_( nop, same )
			on_( '\n' )	narrative_do_( nop, same )
			on_( '/' )	narrative_do_( nop, "/" )
			on_( 'i' )	narrative_do_( read_narrative_in, "in" )
			on_( 'o' )	narrative_do_( read_narrative_on, "on" )
			on_( 'd' )	narrative_do_( read_narrative_do, "do" )
			on_( 't' )	narrative_do_( read_narrative_then, "then" )
			on_other	narrative_do_( error, same)
			end

		in_( "/" ) bgn_
			on_( ' ' )	narrative_do_( nop, same )
			on_( '\t' )	narrative_do_( nop, same )
			on_( '\n' )	narrative_do_( nothing, pop_state )
			on_other	narrative_do_( error, base )
			end

		in_( "in" ) bgn_
			on_( ' ' )	narrative_do_( nop, same )
			on_( '\t' )	narrative_do_( nop, same )
			on_( '(' )	narrative_do_( nop, "in (" )
			on_other	narrative_do_( read_narrative_condition, "in_" )
			end
			in_( "in_" ) bgn_
				on_( ' ' )	narrative_do_( set_narrative_condition, "in (_)" )
				on_( '\t' )	narrative_do_( set_narrative_condition, "in (_)" )
				on_( '\n' )	narrative_do_( set_narrative_condition, same )
						narrative_do_( push_narrative_condition, base )
				on_( 'i' )	narrative_do_( set_narrative_condition, same )
						narrative_do_( read_narrative_in, "in" )
				on_( 'o' )	narrative_do_( set_narrative_condition, same )
						narrative_do_( read_narrative_on, "on" )
				on_( 'd' )	narrative_do_( set_narrative_condition, same )
						narrative_do_( read_narrative_do, "do" )
				on_other	narrative_do_( error, same )
				end
			in_( "in (" ) bgn_
				on_( ' ' )	narrative_do_( nop, same )
				on_( '\t' )	narrative_do_( nop, same )
#if 1	// HACK
				on_( '(' )	narrative_do_( nop, same )	// count++;
				on_( ')' )	narrative_do_( nop, same )	// count--;
#endif
				on_other	narrative_do_( read_narrative_condition, "in (_" )
				end
				in_( "in (_" ) bgn_
					on_( ' ' )	narrative_do_( nop, same )
					on_( '\t' )	narrative_do_( nop, same )
					on_( ')' )	narrative_do_( set_narrative_condition, "in (_)" )
					on_( ',' )	narrative_do_( set_narrative_condition, "in (" )
					on_other	narrative_do_( error, same )
					end
			in_( "in (_)" ) bgn_
				on_( ' ' )	narrative_do_( nop, same )
				on_( '\t' )	narrative_do_( nop, same )
#if 1	// HACK
				on_( ')' )	narrative_do_( nop, same )	// count__;
				on_( ',' )	narrative_do_( nop, "on (" )	// count__;
#endif
				on_( '\n' )	narrative_do_( push_narrative_condition, base )
				on_( 'i' )	narrative_do_( read_narrative_in, "in" )
				on_( 'o' )	narrative_do_( read_narrative_on, "on" )
				on_( 'd' )	narrative_do_( read_narrative_do, "do" )
				on_other	narrative_do_( error, same )
				end

		in_( "on" ) bgn_
			on_( ' ' )	narrative_do_( nop, same )
			on_( '\t' )	narrative_do_( nop, same )
			on_( '(' )	narrative_do_( nop, "on (" )
			on_other	narrative_do_( read_narrative_event, "on_" )
			end
			in_( "on_" ) bgn_
				on_( ' ' )	narrative_do_( set_narrative_event, "on (_)" )
				on_( '\t' )	narrative_do_( set_narrative_event, "on (_)" )
				on_( '\n' )	narrative_do_( set_narrative_event, same )
						narrative_do_( push_narrative_event, base )
				on_( 'i' )	narrative_do_( set_narrative_event, same )
						narrative_do_( read_narrative_in, "in" )
				on_( 'o' )	narrative_do_( set_narrative_event, same )
						narrative_do_( read_narrative_on, "on" )
				on_( 'd' )	narrative_do_( set_narrative_event, same )
						narrative_do_( read_narrative_do, "do" )
				on_other	narrative_do_( error, same )
				end
			in_( "on (" ) bgn_
				on_( ' ' )	narrative_do_( nop, same )
				on_( '\t' )	narrative_do_( nop, same )
#if 1	// HACK
				on_( '(' )	narrative_do_( nop, same )	// count++;
				on_( ')' )	narrative_do_( nop, same )	// count--;
#endif
				on_other	narrative_do_( read_narrative_event, "on (_" )
				end
				in_( "on (_" ) bgn_
					on_( ' ' )	narrative_do_( nop, same )
					on_( '\t' )	narrative_do_( nop, same )
					on_( ')' )	narrative_do_( set_narrative_event, "on (_)" )
					on_( ',' )	narrative_do_( set_narrative_event, "on (" )
					on_other	narrative_do_( error, same )
					end
			in_( "on (_)" ) bgn_
				on_( ' ' )	narrative_do_( nop, same )
				on_( '\t' )	narrative_do_( nop, same )
#if 1	// HACK
				on_( ')' )	narrative_do_( nop, same )	// count__;
				on_( ',' )	narrative_do_( nop, "on (" )	// count__;
#endif
				on_( '\n' )	narrative_do_( push_narrative_event, base )
				on_( 'i' )	narrative_do_( read_narrative_in, "in" )
				on_( 'o' )	narrative_do_( read_narrative_on, "on" )
				on_( 'd' )	narrative_do_( read_narrative_do, "do" )
				on_other	narrative_do_( error, same )
				end

		in_( "do" ) bgn_
			on_( ' ' )	narrative_do_( nop, same )
			on_( '\t' )	narrative_do_( nop, same )
			on_( '\n' )	narrative_do_( push_narrative_action, base )
			on_other	narrative_do_( read_narrative_action, "do_" )
			end
			in_( "do_" ) bgn_
				on_( ' ' )	narrative_do_( nop, same )
				on_( '\t' )	narrative_do_( nop, same )
				on_( '\n' )	narrative_do_( set_narrative_action, base )
				on_( 't' )	narrative_do_( set_narrative_action, same )
						narrative_do_( read_narrative_then, "then" )
				on_other	narrative_do_( error, same )
				end

		in_( "then" ) bgn_
			on_( ' ' )	narrative_do_( set_narrative_then, "then_" )
			on_( '\t' )	narrative_do_( set_narrative_then, "then_" )
			on_( '\n' )	narrative_do_( set_narrative_then, same )
					narrative_do_( push_narrative_then, base )
			on_other	narrative_do_( restore_narrative_action, base )
			end
			in_( "then_" ) bgn_
				on_( 'i' )	narrative_do_( read_narrative_in, "in" )
				on_( 'o' )	narrative_do_( read_narrative_on, "on" )
				on_( 'd' )	narrative_do_( read_narrative_do, "do" )
				on_other	narrative_do_( restore_narrative_action, base )
				end
		end
	}

	narrative_do_( narrative_exit, same );
	return event;
}

