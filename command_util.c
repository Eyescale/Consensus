#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <unistd.h>

#include "database.h"
#include "registry.h"
#include "kernel.h"

#include "command_util.h"
#include "frame.h"
#include "input.h"
#include "input_util.h"
#include "output.h"
#include "io.h"
#include "api.h"
#include "expression.h"
#include "expression_util.h"
#include "narrative.h"
#include "narrative_util.h"
#include "variable.h"
#include "variable_util.h"
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
	if ( !command_mode( 0, 0, ExecutionMode ) )
		return 0;

	freeListItem( &context->expression.results );
	context->expression.results = newItem( CN.nil );
	return 0;
}

int
set_expression_mode( char *state, int event, char **next_state, _context *context )
{
	if ( !command_mode( 0, InstructionMode, ExecutionMode ) )
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
	if ( !command_mode( 0, 0, ExecutionMode ) )
		return 0;

	int retval = expression_solve( context->expression.ptr, 3, context );
	if ( retval == -1 ) return outputf( Error, NULL );

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
	switch ( context->command.mode ) {
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
	if ( command_mode( FreezeMode, 0, 0 ) ) {
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
	else if ( registryLookup( &n->instances, e ) != NULL )
	{
#ifdef DO_LATER
		// output in full: %[ e ].name() - cf. output_narrative_name()
#endif
		outputf( Warning, "narrative '%s' is active - cannot override", name );
		return 0;
	}
	else if ( ttyu( context ) )	// let's talk...
	{
#ifdef DO_LATER
		// output in full: %[ e ].name() - cf. output_narrative_name()
#endif
		outputf( Enquiry, "narrative '%s' already exists. Overwrite ? (y/n)_ ", name );
		int override = 0;
		do {
			override = getchar();
			switch ( override ) {
			case '\n':
				override = 0;
				break;
			case 'y':
			case 'n':
				if ( getchar() == '\n' )
					break;
			default:
				override = 0;
				while ( getchar() != '\n' );
				break;
			}
		}
		while ( override ? 0 : !outputf( Enquiry, "Overwrite ? (y/n)_ " ) );
		if ( override == 'n' ) {
			return 0;
		}
		removeNarrative( e, n );
	}
	else
		return 0;

	return 1;
}

int
narrative_op( char *state, int event, char **next_state, _context *context )
{
	if ( !command_mode( 0, 0, ExecutionMode ) )
		return 0;

	switch ( context->expression.mode ) {
	case InstantiateMode:
		; listItem *last_i = NULL, *next_i;
		for ( listItem *i = context->expression.results; i!=NULL; i=next_i ) {
			next_i = i->next;
			if ( !override_narrative(( Entity *) i->ptr, context ) )
			{
				clipListItem( &context->expression.results, i, last_i, next_i );
			}
			else last_i = i;
		}
		if ( context->expression.results == NULL ) {
			output( Warning, "no target for narrative operation - ignoring" );
			if (( context->input.stack )) {
				output( Info, "closing stream..." );
		                InputVA *input = (InputVA *) context->input.stack->ptr;
				input->shed = 1;
			}
			return 0;
		}

		read_narrative( state, event, next_state, context );

		Narrative *narrative = context->narrative.current;
		context->narrative.current = NULL;

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
exit_command( char *state, int event, char **next_state, _context *context )
{
	if ( !command_mode( 0, InstructionMode, ExecutionMode ) )
		return 0;

	switch ( context->command.mode ) {
	case InstructionMode:
		if ( context->narrative.mode.editing && context->command.block )
			return output( Error, "'exit' not supported in instruction block - "
				"use 'do exit' action instead" );
		break;
	case ExecutionMode:
		if (( context->narrative.current )) {
			context->narrative.current->deactivate = 1;
		}
		break;
	default:
		break;
	}
	return 0;
}

/*---------------------------------------------------------------------------
	set_assignment_mode
---------------------------------------------------------------------------*/
int
set_assignment_mode( char *state, int event, char **next_state, _context *context )
{
	if ( !command_mode( 0, 0, ExecutionMode ) )
		return 0;
	bgn_
	in_( "> identifier" )	context->assignment.mode = AssignAdd;
	in_( ": identifier" )	context->assignment.mode = AssignSet;
	end
	return 0;
}

/*---------------------------------------------------------------------------
	input_inline action
---------------------------------------------------------------------------*/
int
input_inline( char *state, int event, char **next_state, _context *context )
{
	switch ( context->command.mode ) {
	case FreezeMode:
		return 0;
	case InstructionMode:
		// sets execution flag so that the last instruction is parsed again,
		// but this time in execution mode
		set_command_mode( ExecutionMode, context );
		return push_input( "", NULL, InstructionInline, context );
	case ExecutionMode:
		; char *identifier = context->identifier.id[ 0 ].ptr;
		context->identifier.id[ 0 ].ptr = NULL;
		return push_input( "", identifier, PipeInput, context );
	}
}

/*---------------------------------------------------------------------------
	story actions
---------------------------------------------------------------------------*/
int
input_story( char *state, int event, char **next_state, _context *context )
{
	if ( !command_mode( 0, 0, ExecutionMode ) )
		return 0;

	char *path = context->identifier.path;
	context->identifier.path = NULL;

	return push_input( "", path, FileInput, context );
}

int
output_story( char *state, int event, char **next_state, _context *context )
{
	// DO_LATER
	return 0;
}

/*---------------------------------------------------------------------------
	command output actions
---------------------------------------------------------------------------*/
int
set_output_from_variable( char *state, int event, char **next_state, _context *context )
{
	context->output.marked = 0;
	context->output.html = 0;
	if ( !command_mode( 0, 0, ExecutionMode ) )
		return 0;

	char *session, *identifier = context->identifier.id[ 0 ].ptr;
	VariableVA *variable = lookupVariable( context, identifier, 0 );
	if (( variable == NULL ) || ( variable->value == NULL ))
		return outputf( Error, ">@ %%variable: variable '%s' not found", identifier );

#ifdef DO_LATER
	// for now only set the output to the first variable's entity's name
#endif
	switch ( variable->type ) {
	case EntityVariable:
		session = cn_name( ((listItem *) variable->value )->ptr );
		if ( identifier == NULL )
			return outputf( Error, ">@ %%variable: variable '%s'\'s value is a relationship instance" );
		break;
	default:
		return outputf( Error, ">@ %%variable: variable '%s' is not an entity variable" );
	}

	free( context->identifier.path );
	context->identifier.path = session;
	return set_output_target( ">@session", ':', &same, context );
}

int
set_output_target( char *state, int event, char **next_state, _context *context )
{
	context->output.marked = 0;
	context->output.html = 0;
	event = 0;
	if ( !command_mode( 0, 0, ExecutionMode ) )
		return 0;

	if ( !strcmp( state, ": identifier" ) ) {
		char *variable = context->identifier.id[ 0 ].ptr;
		context->identifier.id[ 0 ].ptr = NULL;
		event = push_output( "^^", variable, StringOutput, context );
	}
	else if ( !strcmp( state, ">@session" ) ) {
		char *path = context->identifier.path;
		event = push_output( "^^", path, SessionOutput, context );
	}
	else if ( !strcmp( state, "> .." ) ) {
		if (( context->input.stack )) {
			InputVA *input = context->input.stack->ptr;
			if ( input->mode == ClientInput ) {
				event = push_output( "^^", &input->client, ClientOutput, context );
				if ( !event ) { context->io.query.sync = 1; return 0; }
			}
		}
		return output( Error, "> .. : no client input" );
	}
	return event;
}

int
sync_delivery( char *state, int event, char **next_state, _context *context )
{
	if ( !command_mode( 0,0, ExecutionMode ) )
		return 0;
	if (( context->input.stack )) {
		InputVA *input = context->input.stack->ptr;
		if (( input->mode == ClientInput ) && ( input->session.created ))
		{
			// genitor wants to sync on iam
			input->session.created->genitor = input->client;
			context->io.query.sync = 0;
			context->io.query.leave_open = 1;
		}
		else context->io.query.sync = 1;
	}
	return 0;
}

int
sync_output( char *state, int event, char **next_state, _context *context )
{
	if ( !command_mode( 0,0, ExecutionMode ) )
		return 0;
	if ( !context->output.marked )
		return '\n';

	output( Text, "\n" );
	if ( !context->output.redirected )
		return 0;
	if  ( context->io.query.sync )
		return 0;

	context->io.query.sync = 1;
	output( Text, "|" );
	return 0;
}

int
backfeed_output( char *state, int event, char **next_state, _context *context )
{
	if ( !command_mode( 0, 0, ExecutionMode ) )
		return 0;

	if ( context->output.query && !context->output.marked ) {
		context->io.query.sync = 1;
		output( Text, ">..:" );
	} else {
		return output_char( state, event, next_state, context );
	}
	return 0;
}

static int
pipe_output( InputType into, _context *context )
{
	if ( !command_mode( 0, 0, ExecutionMode ) )
		return 0;

	context->output.marked = 0;
	if ( !context->output.redirected )
		return 0;

	OutputVA *output = context->output.stack->ptr;
	int *fd = &output->ptr.socket;
	pop_output( context, 0 );
	push_input( "", fd, into, context );
	return 0;
}

int
pipe_output_in( char *state, int event, char **next_state, _context *context )
{
	return pipe_output( SessionOutputPipe, context );
}

int
pipe_output_out( char *state, int event, char **next_state, _context *context )
{
	return pipe_output( SessionOutputPipeOut, context );
}

int
clear_output_target( char *state, int event, char **next_state, _context *context )
{
	int close_client_ticket = context->output.html;
	context->output.html = 0;
	context->output.marked = 0;

	if ( !command_mode( 0, 0, ExecutionMode ) )
		return event;
	if ( !context->output.redirected )
		return event;

	OutputVA *output = context->output.stack->ptr;
	switch ( output->mode ) {
	case SessionOutput:
		pop_output( context, 1 );
		break;
	case ClientOutput:
		pop_output( context, close_client_ticket );
		break;
	case StringOutput:
		pop_output( context, 0 );
		break;
	}
	return event;
}

/*---------------------------------------------------------------------------
	create_session
---------------------------------------------------------------------------*/
static int
create_session( char *path, _context *context )
{
	// verify that a session with that name does not already exists
	registryEntry *entry = registryLookup( context->operator.sessions, path );
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
	session->pid = fork();
	if ( !session->pid ) {
		// child session will call io_init() and register itself
#ifdef DO_LATER
		// additional optional arguments: "-hcn", filename, "-story", filename
#endif
		char *exe = context->session.path;
		execlp( exe, exe, "-msession", path, (char *)0 );

		// execlp should not return at all, but in case:
		// here we are in the child process. We must send back a message to operator.
		// but actually operator should catch exit() with SIGCHLD.
		outputf( Error, "execlp: failure to create session %s", path );
		exit( -1 );
	}
	registryRegister( context->operator.sessions, session->path, session );
	registryRegister( context->operator.pid, &session->pid, session );

	if (( context->input.stack )) {
		// carry session in case genitor wants to sync
		InputVA *input = context->input.stack->ptr;
		input->session.created = session;
	}
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

	char *path = context->identifier.path;
	char *pid = context->identifier.id[ 2 ].ptr;

	registryEntry *entry = registryLookup( context->operator.sessions, path );
	if ( entry == NULL )
		return outputf( Warning, "iam notification: path unknown: '%s'", path );

	SessionVA *session = entry->value;
	if ( session->pid != atoi( pid ) )
		return outputf( Warning, "iam event: unable to authenticate: '%s' - ignoring", path );

	// create relationship instance
	cn_instf( "...", path, "is", "session" );

	if ( session->genitor )
	{
		char *news;
		asprintf( &news, "< operator >: session:%s !!", path );

		int fd = session->genitor;
		int remainder = 0, length = strlen( news );
		io_write( fd, news, length, &remainder );
		io_flush( fd, &remainder, 0, context );
		write( fd, &remainder, sizeof( remainder ) );
		close( fd );

		session->genitor = 0;
		free( news );
	}
	return 0;
}

/*---------------------------------------------------------------------------
	scheme_op	- operator specific
---------------------------------------------------------------------------*/
/*
	implements !! session://path	- operator only
*/
int
scheme_op( char *state, int event, char **next_state, _context *context )
{
	char *scheme = context->identifier.scheme;
	if ( strcmp( scheme, "session" ) )
		return outputf( Error, "%s operation not supported", scheme );
	if ( context->expression.mode != InstantiateMode )
		return output( Error, "session operation not supported" );
#ifdef DO_LATER
	check session's path format (must not be a number)
#endif
	if ( !command_mode( 0, 0, ExecutionMode ) )
		return 0;
	if ( !context->control.operator )
		return output( Error, "session operation not authorized" );

	char *path = context->identifier.path;
	int retval = create_session( path, context );
	if ( !retval )
		context->identifier.path = NULL;

	return 0;
}

/*---------------------------------------------------------------------------
	input_cgi
---------------------------------------------------------------------------*/
static int
add_cgi_entry( char *state, int event, char **next_state, _context *context )
{
	Expression *expression = makeLiteral( context->expression.ptr );
	if (( expression )) {
		EntityLog *log = &context->frame.log.cgi;
		addItem( &log->instantiated, &expression->sub[ 3 ] );
		context->expression.ptr = NULL;
	}
	return 0;
}

static void
free_cgi_log( _context *context )
{
	EntityLog *log = &context->frame.log.cgi;
	for ( listItem *i=log->instantiated; i!=NULL; i=i->next )
		freeLiteral( i->ptr );
	freeListItem( &log->instantiated );
}

int
input_cgi( char *state, int event, char **next_state, _context *context )
{
	if ( !context->control.session )
		return output( Error, "< cgi >: not in a session" );
	if ( !command_mode( 0, 0, ExecutionMode ) )
		return output( Error, "< cgi >: not in execution mode" );

	// build the log of cgi entries
	// ----------------------------
	event = 0;
	state = "< cgi >:";
	do {
		event = read_input( state, event, &same, context );
		bgn_
		on_( -1 )	do_( nothing, "" )
		on_( ' ' )	do_( nop, same )
		on_( '\t' )	do_( nop, same )
		in_( "< cgi >:" ) bgn_
			on_( '{' )	do_( nop, "< cgi >: {" )
			on_other	do_( read_expression, "< cgi >:_" )
			end
		in_( "< cgi >:_" ) bgn_
			on_( '\n' )	do_( add_cgi_entry, "" )
			on_other	do_( error, "" )
			end
		in_( "< cgi >: {" ) bgn_
			on_any		do_( read_expression, "< cgi >: {_" )
			end
			in_( "< cgi >: {_" ) bgn_
				on_( ',' )	do_( add_cgi_entry, "< cgi >: {" )
				on_( '}' )	do_( add_cgi_entry, "< cgi >: {_}" )
				on_other	do_( error, "" )
				end
			in_( "< cgi >: {_}" ) bgn_
				on_( '\n' )	do_( nop, "" )
				on_other	do_( error, "" )
				end
		end
	}
	while ( strcmp( state, "" ) );
	if ( event < 0 )
		free_cgi_log( context );

	// process cgi entries
	// -------------------
	EntityLog *log = &context->frame.log.cgi;
	if ( log->instantiated == NULL )
		return event;

	Narrative *n = lookupNarrative( CN.nil, "cgi" );
	if ( n == NULL ) {
		event = outputf( Error, "session: %s - cgi story not found", context->session.path );
	}
	else {
		activateNarrative( CN.nil, n );
		do { narrative_process( n, log, context ); }
		while ( !n->deactivate );
		deactivateNarrative( CN.nil, n );
	}

	free_cgi_log( context );
	return event;
}

