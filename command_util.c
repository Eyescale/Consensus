#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/select.h>

#include "database.h"
#include "registry.h"
#include "kernel.h"

#include "input.h"
#include "input_util.h"
#include "output.h"
#include "io.h"
#include "api.h"
#include "expression.h"
#include "narrative.h"
#include "variables.h"
#include "value.h"

// #define DEBUG

/*---------------------------------------------------------------------------
	execution engine	- local
---------------------------------------------------------------------------*/
#define do_( a, s ) \
	event = execute( a, &state, event, s, context );

static int
execute( _action action, char **state, int event, char *next_state, _context *context )
{
	int retval;
	if ( action == push ) {
		event = push( *state, event, &next_state, context );
		*state = base;
	} else {
		event = action( *state, event, &next_state, context );
		if ( strcmp( next_state, same ) )
			*state = next_state;
	}
	return event;
}

/*---------------------------------------------------------------------------
	command expression actions
---------------------------------------------------------------------------*/
int
set_results_to_nil( char *state, int event, char **next_state, _context *context )
{
	if ( !context_check( 0, 0, ExecutionMode ) )
		return 0;

	freeListItem( &context->expression.results );
	context->expression.results = newItem( CN.nil );
	return 0;
}

int
set_expression_mode( char *state, int event, char **next_state, _context *context )
{
	if ( !context_check( 0, InstructionMode, ExecutionMode ) )
		return 0;

	switch ( event ) {
	case '!': context->expression.mode = InstantiateMode; break;
	case '~': context->expression.mode = ReleaseMode; break;
	case '*': context->expression.mode = ActivateMode; break;
	case '_': context->expression.mode = DeactivateMode; break;
	}
	return 0;
}

int
expression_op( char *state, int event, char **next_state, _context *context )
{
	if ( !context_check( 0, 0, ExecutionMode ) )
		return 0;

	int retval = expression_solve( context->expression.ptr, 3, context );
	if ( retval == -1 ) return output( Error, NULL );

	switch ( context->expression.mode ) {
	case InstantiateMode:
		// already done during expression_solve
		break;
	case ReleaseMode:
		for ( listItem *i = context->expression.results; i!=NULL; i=i->next ) {
			Entity *e = (Entity *) i->ptr;
			cn_release( e );
		}
		freeListItem( &context->expression.results );
		break;
	case ActivateMode:
		for ( listItem *i = context->expression.results; i!=NULL; i=i->next ) {
			Entity *e = (Entity *) i->ptr;
			cn_activate( e );
		}
		break;
	case DeactivateMode:
		for ( listItem *i = context->expression.results; i!=NULL; i=i->next ) {
			Entity *e = (Entity *) i->ptr;
			cn_deactivate( e );
		}
		break;
	default:
		break;
	}
	return event;
}

int
evaluate_expression( char *state, int event, char **next_state, _context *context )
{
	switch ( context->control.mode ) {
	case FreezeMode:
		do_( flush_input, same );
		return event;
	case InstructionMode:
		do_( read_expression, same );
		return event;
	case ExecutionMode:
		break;
	}

	int restore_mode = 0;
	context->expression.do_resolve = 1;
	bgn_
	/*
	   the mode will be needed by narrative_op
	*/
	in_( "!. %[" )			restore_mode = context->expression.mode;
	in_( ": identifier : !. %[" )	restore_mode = context->expression.mode;
	/*
	   in loop or condition evaluation we do not want expression_solve() to
	   resolve filtered results if there are, and unless specified otherwise.
	   Note that expression_solve may revert to EvaluateMode.
	*/
	in_( "? identifier:" )		context->expression.do_resolve = 0;
	in_( "? %:" )			context->expression.do_resolve = 0;
	in_( "?~ %:" )			context->expression.do_resolve = 0;
	in_( ": identifier : %[" )	context->expression.do_resolve = 0;	// cf. assign_results, assign_va, assign_narrative
	in_( ">: %[" )			context->expression.do_resolve = 0;	// cf. output_results, output_va, output_narrative
					context->error.flush_output = 1;	// we want to output errors in the text
	end

	do_( read_expression, same );
	if ( context->expression.mode != ErrorMode ) {
		Expression *expression = context->expression.ptr;
		context->expression.mode = EvaluateMode;
		int retval = expression_solve( expression, 3, context );
		if ( retval < 0 ) event = retval;
	}
	context->expression.do_resolve = 0;

	if ( restore_mode ) {
		context->expression.mode = restore_mode;
	}
	return event;
}

int
parse_expression( char *state, int event, char **next_state, _context *context )
{
	if ( context_check( FreezeMode, 0, 0 ) ) {
		do_( flush_input, same );
		return event;
	}

	// the mode may be needed by expression_op
	int restore_mode = context->expression.mode;
	do_( read_expression, same );
	context->expression.mode = restore_mode;

	return event;
}

/*---------------------------------------------------------------------------
	command narrative actions
---------------------------------------------------------------------------*/
int
override_narrative( Entity *e, _context *context )
{
	char *name = context->identifier.id[ 1 ].ptr;

	// does e already have a narrative with the same name?
	Narrative *n = lookupNarrative( e, name );
	if ( n == NULL ) return 1;

	// if yes, then is that narrative already active?
	else if ( lookupByAddress( n->instances, e ) != NULL )
	{
#ifdef DO_LATER
		// output in full: %[ e ].name() - cf. output_narrative_name()
#endif
		outputf( Warning, "narrative '%s' is active - cannot overwrite", name );
		return 0;
	}
	else if ( ttyu( context ) )	// let's talk...
	{
#ifdef DO_LATER
		// output in full: %[ e ].name() - cf. output_narrative_name()
#endif
		outputf( Enquiry, "narrative '%s' already exists. Overwrite ? (y/n)_ ", name );
		int overwrite = 0;
		do {
			overwrite = getchar();
			switch ( overwrite ) {
			case '\n':
				overwrite = 0;
				break;
			case 'y':
			case 'n':
				if ( getchar() == '\n' )
					break;
			default:
				overwrite = 0;
				while ( getchar() != '\n' );
				break;
			}
		}
		while ( overwrite ? 0 : !outputf( Enquiry, "Overwrite ? (y/n)_ " ) );
		if ( overwrite == 'n' )
			return 0;

		removeNarrative( e, n );
	}
	else
		return 0;

	return 1;
}

int
narrative_op( char *state, int event, char **next_state, _context *context )
{
	if ( !context_check( 0, 0, ExecutionMode ) )
		return 0;

	switch ( context->expression.mode ) {
	case InstantiateMode:
		; listItem *last_i = NULL, *next_i;
		for ( listItem *i = context->expression.results; i!=NULL; i=i->next ) {
			next_i = i->next;
			if ( !override_narrative(( Entity *) i->ptr, context ) )
			{
				clipListItem( &context->expression.results, i, last_i, next_i );
			}
			else last_i = i;
		}
		if ( context->expression.results == NULL ) {
			output( Warning, "no target for narrative operation - ignoring" );
			return 0;
		}

		read_narrative( state, event, next_state, context );
		Narrative *narrative = context->narrative.current;
		if ( narrative == NULL ) {
			output( Warning, "narrative empty - not instantiated" );
			return 0;
		}
		int do_register = 0;
		for ( listItem *i = context->expression.results; i!=NULL; i=i->next )
		{
			Entity *e = (Entity *) i->ptr;
			if ( cn_instantiate_narrative( e, narrative ) )
				do_register = 1;
		}
		if ( do_register ) {
			registerNarrative( narrative, context );
			outputf( Info, "narrative instantiated: %s()", narrative->name );
		} else {
			freeNarrative( narrative );
			output( Warning, "no target entity - narrative not instantiated" );
		}
		context->narrative.current = NULL;
		break;

	case ReleaseMode:
		; char *name = context->identifier.id[ 1 ].ptr;
		for ( listItem *i = context->expression.results; i!=NULL; i=i->next )
		{
			Entity *e = (Entity *) i->ptr;
			cn_release_narrative( e, name );
		}
		event = 0;
		break;

	case ActivateMode:
		name = context->identifier.id[ 1 ].ptr;
		for ( listItem *i = context->expression.results; i!=NULL; i=i->next )
		{
			Entity *e = (Entity *) i->ptr;
			cn_activate_narrative( e, name );
		}
		event = 0;
		break;

	case DeactivateMode:
		name = context->identifier.id[ 1 ].ptr;
		for ( listItem *i = context->expression.results; i!=NULL; i=i->next )
		{
			Entity *e = (Entity *) i->ptr;
			cn_deactivate_narrative( e, name );
		}
		event = 0;
		break;

	default:
		// only supports these modes for now...
		event = 0;
		break;
	}
	return 0;
}

int
exit_narrative( char *state, int event, char **next_state, _context *context )
{
	switch ( context->control.mode ) {
	case FreezeMode:
		return 0;
	case InstructionMode:
		if ( context->narrative.mode.action.one )
			break;
	case ExecutionMode:
		if ( context->narrative.current == NULL ) {
			return output( Error, "'exit' command only supported in narrative mode" );
		}
		else if ( context->narrative.mode.action.block && ( context->control.mode == InstructionMode ) ) {
			return output( Error, "'exit' is not a supported instruction - use 'do exit' action instead" );
		}
		context->narrative.current->deactivate = 1;
	}
	return event;
}

/*---------------------------------------------------------------------------
	command va actions
---------------------------------------------------------------------------*/
int
read_va( char *state, int event, char **next_state, _context *context )
{
	event = 0;	// we know it is '$'
	state = base;
	do {
		event = input( state, event, NULL, context );
#ifdef DEBUG
		output( Debug, "read_va: in \"%s\", on '%c'", state, event );
#endif
		bgn_
		in_( base ) bgn_
			on_( '(' )	do_( nop, "(" )
			on_other	do_( error, "" )
			end
			in_( "(" ) bgn_
				on_( ' ' )	do_( nop, same )
				on_( '\t' )	do_( nop, same )
				on_other	do_( read_2, "(_" )
				end
				in_( "(_" ) bgn_
					on_( ' ' )	do_( nop, same )
					on_( '\t' )	do_( nop, same )
					on_( ')' )	do_( nop, "" )
					on_other	do_( error, "" )
					end
		end
	}
	while ( strcmp( state, "" ) );

	return event;
}

/*---------------------------------------------------------------------------
	set_assignment_mode
---------------------------------------------------------------------------*/
int
set_assignment_mode( char *state, int event, char **next_state, _context *context )
{
	if ( !context_check( 0, 0, ExecutionMode ) )
		return 0;

	bgn_
	in_( "> identifier" )	context->assignment.mode = AssignAdd;
	in_( ": identifier" )	context->assignment.mode = AssignSet;
	end

	return 0;
}

/*---------------------------------------------------------------------------
	command input action
---------------------------------------------------------------------------*/
int
input_command( char *state, int event, char **next_state, _context *context )
{
	switch ( context->control.mode ) {
	case FreezeMode:
		return 0;
	case InstructionMode:
		// sets execution flag so that the last instruction is parsed again,
		// but this time in execution mode
		set_control_mode( ExecutionMode, context );
		return push_input( "", NULL, InstructionInline, context );
	case ExecutionMode:
		break;
	}
	char *identifier = context->identifier.id[ 0 ].ptr;
	context->identifier.id[ 0 ].ptr = NULL;

	return push_input( "", identifier, PipeInput, context );
}

/*---------------------------------------------------------------------------
	command output actions
---------------------------------------------------------------------------*/
int
set_output_target( char *state, int event, char **next_state, _context *context )
{
	if ( !context_check( 0, 0, ExecutionMode ) )
		return 0;

	if ( !strcmp( state, ": identifier" ) ) {
		char *variable = context->identifier.id[ 0 ].ptr;
		context->identifier.id[ 0 ].ptr = NULL;
		push_output( "^^", variable, StringOutput, context );
	}
	else if ( !strcmp( state, ">@session" ) ) {
		char *path = context->identifier.id[ 1 ].ptr;
		push_output( "^^", path, SessionOutput, context );
	}
	else if ( !strcmp( state, "> .." ) ) {
		push_output( "^^", &context->io.client, ClientOutput, context );
		context->io.query.sync = 1;
	}
	return 0;
}

int
output_backfeed( char *state, int event, char **next_state, _context *context )
{
	if ( !context_check( 0, 0, ExecutionMode ) )
		return 0;

	if ( context->output.flag.query && !context->output.flag.marked ) {
		context->io.query.sync = 1;
		output( Text, ">..:" );
	} else {
		return output_char( state, event, next_state, context );
	}
	return 0;
}

static int
output_pipe( InputType into, _context *context )
{
	if ( !context_check( 0, 0, ExecutionMode ) )
		return 0;

	context->output.flag.marked = 0;
	if ( !context->output.flag.redirected )
		return output( Error, "pipe format not supported" );

	OutputVA *output = context->output.stack->ptr;
	int *fd = &output->ptr.socket;
	pop_output( context, 0 );
	context->io.query.sync = 0;
	push_input( "", fd, into, context );
	return 0;
}

int
service_sync( char *state, int event, char **next_state, _context *context )
{
	if ( !context_check( 0,0, ExecutionMode ) )
		return 0;
	if (( context->input.stack )) {
		InputVA *input = context->input.stack->ptr;
		if ( input->mode == ClientInput )
			context->io.query.sync = 1;
	}
	return 0;
}

int
output_sync( char *state, int event, char **next_state, _context *context )
{
	if ( !context_check( 0,0, ExecutionMode ) )
		return 0;

	context->output.flag.marked = 0;
	if ( !context->output.flag.redirected )
		return 0;

	context->io.query.sync = 1;
	output( Text, "|" );
	pop_output( context, 1 );
	return 0;
}

int
output_pipe_in( char *state, int event, char **next_state, _context *context )
{
	return output_pipe( SessionInput, context );
}

int
output_pipe_out( char *state, int event, char **next_state, _context *context )
{
	return output_pipe( SessionPipeInputOut, context );
}

int
clear_output_target( char *state, int event, char **next_state, _context *context )
{
	if ( !context_check( 0, 0, ExecutionMode ) )
		return 0;

	context->output.flag.marked = 0;
	if ( !context->output.flag.redirected )
		return 0;

	OutputVA *output = context->output.stack->ptr;
	switch ( output->mode ) {
	case SessionOutput:
		pop_output( context, 1 );
		break;
	case ClientOutput:
		pop_output( context, 0 );
		break;
	case StringOutput:
		pop_output( context, 0 );
		break;
	}
	return 0;
}

/*---------------------------------------------------------------------------
	create_session
---------------------------------------------------------------------------*/
static int
create_session( char *path, _context *context )
{
	// verify that a session with that name does not already exists
	registryEntry *entry = lookupByName( context->operator.sessions, path );
	if ( entry != NULL ) {
#ifdef LATER
		// must notify genitor
#endif
		return outputf( Error, "session %s already exists - cannot create", path );
	}

#ifdef DO_LATER
	// create session directory under /Library/WebServer/Documents/consensus/
#endif

	SessionVA *session = calloc( 1, sizeof(SessionVA) );
	session->path = path;
	session->operator = getpid();
	session->genitor.query = context->io.client;
	session->pid = fork();
	if ( !session->pid ) {
		// child session will call io_init() and register itself
#ifdef DO_LATER
		// additional optional arguments: "-hcn", filename, "-story", filename
#endif
		execlp( CN_EXE_PATH, CN_EXE_PATH, "-msession", path, NULL );
		// execlp should not return at all, but in case:
		// here we are in the child process. We must send back a message to operator.
		// but actually operator should catch exit() with SIGCHLD.
		outputf( Error, "execlp: failure to create session %s", path );
		exit( -1 );
	}
	registerByName( &context->operator.sessions, session->path, session );
	registerByIndex( &context->operator.pid, session->pid, session );

	// leave genitor hanging until session's iam notification
	if ( context->io.client )
		context->io.query.sync = 0;
	return 0;
}

/*---------------------------------------------------------------------------
	handle_iam
---------------------------------------------------------------------------*/
int
handle_iam( char *state, int event, char **next_state, _context *context )
{
	if ( !context->control.operator )
		return output( Warning, "iam event: wrong address" );

	char *path = context->identifier.id[ 1 ].ptr;
	char *pid = context->identifier.id[ 2 ].ptr;

	registryEntry *entry = lookupByName( context->operator.sessions, path );
	if ( entry == NULL )
		return outputf( Warning, "iam notification: path unknown: '%s'", path );

	SessionVA *session = entry->value;
	if ( session->pid != atoi( pid ) )
		return outputf( Warning, "iam event: unable to authenticate: '%s' - ignoring", path );

	if ( session->genitor.query > 0 )
	{
		char *news;
		asprintf( &news, "< operator >: session:%s !!", path );

		int fd = session->genitor.query, remainder = 0, length = strlen( news );
		io_write( fd, news, length, &remainder );
		io_flush( fd, &remainder, context );
		io_close( IO_QUERY, fd, "" );
		session->genitor.query = 0;

		free( news );
	}
	return 0;
}

/*---------------------------------------------------------------------------
	scheme_op
---------------------------------------------------------------------------*/
int
scheme_op( char *state, int event, char **next_state, _context *context )
{
	char *scheme = context->expression.scheme;
	if ( !strcmp( scheme, "session" ) )
	{
		if ( !context->control.operator )
			return output( Error, "session operation not authorized" );
		else if ( context->expression.mode != InstantiateMode )
			return outputf( Warning, "session operation not supported - ignoring", scheme );

		if ( !context_check( 0, 0, ExecutionMode ) )
			return 0;

		switch ( context->expression.mode ) {
		case InstantiateMode:
			; char *path = context->identifier.id[ 1 ].ptr;
			context->identifier.id[ 1 ].ptr = NULL;
			event = create_session( path, context );
			if ( event ) free( path );
			break;
		default:
			break;
		}
	}
	else outputf( Warning, "%s scheme operation not supported - ignoring", scheme );
	return 0;
}

/*---------------------------------------------------------------------------
	stream actions
---------------------------------------------------------------------------*/
int
open_stream( char *state, int event, char **next_state, _context *context )
{
	char *path = context->identifier.id[ 1 ].ptr;
	if ( path == NULL ) return output( Error, "no file specified" );
	bgn_
	in_( "!< file://path" )		event = cn_open( path, O_RDONLY );
	in_( "!< file://path <" )	event = cn_open( path, O_RDONLY );
	in_( "!< file://path >" )	event = cn_open( path, O_WRONLY );
	in_( "!< file://path ><" )	event = cn_open( path, O_RDWR );
	end
	return event;
}

int
input_story( char *state, int event, char **next_state, _context *context )
{
	if ( !context_check( 0, 0, ExecutionMode ) )
		return 0;

	char *identifier = context->identifier.id[ 1 ].ptr;
	context->identifier.id[ 1 ].ptr = NULL;

	return push_input( "", identifier, FileInput, context );
}

int
output_story( char *state, int event, char **next_state, _context *context )
{
	// DO_LATER
	return 0;
}

