#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#include "database.h"
#include "registry.h"
#include "kernel.h"

#include "command_util.h"
#include "frame.h"
#include "input.h"
#include "input_util.h"
#include "output.h"
#include "output_util.h"
#include "io.h"
#include "api.h"
#include "expression.h"
#include "expression_util.h"
#include "narrative.h"
#include "narrative_util.h"
#include "variable.h"
#include "variable_util.h"
#include "value.h"
#include "xl.h"

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
build_expression( char *state, int event, char **next_state, _context *context )
{
	context->expression.mode = BuildMode;
	return read_expression( state, event, next_state, context );
}
int
dereference_expression( char *state, int event, char **next_state, _context *context )
{
	if ( !command_mode( 0, 0, ExecutionMode ) )
		return build_expression( state, event, next_state, context );

	event = evaluate_expression( state, event, next_state, context );
	if ( event < 0 ) return event;

	if ( context->expression.mode == ReadMode ) {
		for ( listItem *i = context->expression.results; i!=NULL; i=i->next ) {
			Literal *l = i->ptr;
			Expression *e = l->e;
			if ( !e->sub[ 1 ].result.none )
				continue;
			// HTML decode identifier
			char *identifier = e->sub[ 0 ].result.identifier.value;
			char *dst = identifier;
			for ( char *src = dst; *src; src++ ) {
#if 1
				if ( !strncmp( src, "%22", 3 ) ) {
					src += 2;
					*dst++ = '\"';
				}
#else
				if ( *src == '%' ) {
					char hex[ 3 ];
					hex[ 0 ] = *++src;
					hex[ 1 ] = *++src;
					hex[ 2 ] = 0;
					*dst++ = strtol( hex, NULL, 16 );
				}
#endif
				else *dst++ = *src;
			}
			while ( *dst ) *dst++ = ' ';
			e = s2x( e->sub[ 0 ].result.identifier.value, context );
			if ( e == NULL ) continue;
			freeLiteral( i->ptr );
			i->ptr = xl( e );
		}
	}
	return event;
}
int
assimilate_expression( char *state, int event, char **next_state, _context *context )
{
	if ( !command_mode( 0, 0, ExecutionMode ) )
		return build_expression( state, event, next_state, context );

	context->expression.mode = 0;
	return read_expression( state, event, next_state, context );
}
int
evaluate_expression( char *state, int event, char **next_state, _context *context )
{
	if ( !command_mode( 0, 0, ExecutionMode ) )
		return build_expression( state, event, next_state, context );

	context->expression.mode = BuildMode;
	event = read_expression( state, event, next_state, context );
	if ( event < 0 ) return event;

	context->expression.mode = 0;
	int retval = expression_solve( context->expression.ptr, 3, NULL, context );
	return ( retval < 0 ) ? retval : event;
}
int
test_expression( char *state, int event, char **next_state, _context *context )
{
	if ( !command_mode( 0, 0, ExecutionMode ) )
		return build_expression( state, event, next_state, context );

	StackVA *stack = context->command.stack->ptr;
	switch ( stack->clause.mode ) {
	case ClauseNone:
	case ClauseElse:
	case ActiveClause:
		return evaluate_expression( state, event, next_state, context );
	default:
		set_command_mode( FreezeMode, context );
		event = build_expression( state, event, next_state, context );
		set_command_mode( ExecutionMode, context );
		return event;
	}
}
int
solve_expression( char *state, int event, char **next_state, _context *context )
{
	if ( !command_mode( 0, 0, ExecutionMode ) )
		return build_expression( state, event, next_state, context );

	context->expression.mode = BuildMode;
	event = read_expression( state, event, next_state, context );
	if ( event < 0 ) return event;

	context->expression.mode = EvaluateMode;
	int retval = expression_solve( context->expression.ptr, 3, NULL, context );
	return ( retval < 0 ) ? retval : event;
}
int
expression_op( char *state, int event, char **next_state, _context *context )
{
	if ( !command_mode( 0, 0, ExecutionMode ) )
		return event;

	switch ( context->command.op ) {
	case InstantiateOperation:
		context->expression.mode = InstantiateMode;
		break;
	default:
		context->expression.mode = EvaluateMode;
	}

	int retval = expression_solve( context->expression.ptr, 3, NULL, context );
	if ( retval == -1 ) return outputf( Error, NULL );

	switch ( context->command.op ) {
	case InstantiateOperation:
		// already done during expression_solve
		break;
	case ReleaseOperation:
		for ( listItem *i = context->expression.results; i!=NULL; i=i->next ) {
			Entity *e = (Entity *) i->ptr;
			cn_release( e );
		}
		freeListItem( &context->expression.results );
		break;
	case ActivateOperation:
		for ( listItem *i = context->expression.results; i!=NULL; i=i->next ) {
			Entity *e = (Entity *) i->ptr;
			cn_activate( e );
		}
		break;
	case DeactivateOperation:
		for ( listItem *i = context->expression.results; i!=NULL; i=i->next ) {
			Entity *e = (Entity *) i->ptr;
			cn_deactivate( e );
		}
		break;
	}
	return event;
}
int
set_command_op( char *state, int event, char **next_state, _context *context )
{
	if ( !command_mode( 0, 0, ExecutionMode ) )
		return 0;
	bgn_
	on_( '!' )	context->command.op = InstantiateOperation;
	on_( '~' )	context->command.op = ReleaseOperation;
	on_( '*' )	context->command.op = ActivateOperation;
	on_( '_' )	context->command.op = DeactivateOperation;
	end
	return 0;
}
int
set_results_to_nil( char *state, int event, char **next_state, _context *context )
{
	if ( !command_mode( 0, 0, ExecutionMode ) )
		return 0;

	freeListItem( &context->expression.results );
	context->expression.results = newItem( CN.nil );
	context->expression.mode = EvaluateMode;
	return 0;
}

/*---------------------------------------------------------------------------
	command narrative actions
---------------------------------------------------------------------------*/
int
override_narrative( Entity *e, _context *context )
{
	char *name = get_identifier( context, 1 );

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

	switch ( context->command.op ) {
	case InstantiateOperation:
		; listItem *last_i = NULL, *next_i;
		for ( listItem *i = context->expression.results; i!=NULL; i=next_i )
		{
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
				input->discard = 1;
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
		for ( listItem *i = context->expression.results; i!=NULL; i=i->next ) {
			Entity *e = (Entity *) i->ptr;
			if ( cn_instantiate_narrative( e, narrative ) )
				do_register = 1;
		}
		if ( do_register ) {
			registerNarrative( narrative, context );
			outputf( Info, "narrative instantiated: %s()", narrative->name );
		}
		else {
			freeNarrative( narrative );
			output( Warning, "no target entity - narrative not instantiated" );
		}
		break;
	case ReleaseOperation:
		; char *name = get_identifier( context, 1 );
		for ( listItem *i = context->expression.results; i!=NULL; i=i->next ) {
			Entity *e = (Entity *) i->ptr;
			cn_release_narrative( e, name );
		}
		break;
	case ActivateOperation:
		name = get_identifier( context, 1 );
		for ( listItem *i = context->expression.results; i!=NULL; i=i->next ) {
			Entity *e = (Entity *) i->ptr;
			cn_activate_narrative( e, name );
		}
		break;
	case DeactivateOperation:
		name = get_identifier( context, 1 );
		for ( listItem *i = context->expression.results; i!=NULL; i=i->next ) {
			Entity *e = (Entity *) i->ptr;
			cn_deactivate_narrative( e, name );
		}
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
		if ( context->narrative.mode.editing && !context->command.one )
			return output( Error, "'exit' not supported in instruction block - "
				"use 'do exit' action instead" );
		break;
	case ExecutionMode:
		if (( context->narrative.current )) {
			context->narrative.current->deactivate = 1;
			context->output.cgi = 0;
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
	on_( '+' )	context->assignment.mode = AssignAdd;
	on_( ':' )	context->assignment.mode = AssignSet;
	on_( '<' )	context->assignment.mode = AssignSet;
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
		; char *identifier = take_identifier( context, 0 );
		return push_input( "", identifier, UNIXPipeInput, context );
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
set_output_target( char *state, int event, char **next_state, _context *context )
{
	context->output.marked = 0;
	event = 0;
	if ( !command_mode( 0, 0, ExecutionMode ) )
		return event;

	if ( !strcmp( state, "> identifier" ) ) {
		char *variable = take_identifier( context, 0 );
		event = push_output( "^^", variable, StringOutput, context );
	}
	else if ( !strcmp( state, ">@session" ) ) {
		char *path = context->identifier.path;
		if (( path )) {
			context->identifier.path = 0;
			event = push_output( "^^", path, SessionOutput, context );
			free( path );
		}
		// otherwise (null path) will default to stdout
	}
	else if ( !strcmp( state, ">< file:path >" ) ) {
		char *path = context->identifier.path;
		if (( path )) {
			context->identifier.path = 0;
			event = push_output( "^^", path, FileOutput, context );
			free( path );
		}
	}
	else if ( !strcmp( state, "> .." ) ) {
		InputVA *input;
		if (( context->input.stack ) &&
		    (( input = context->input.stack->ptr )->mode == ClientInput ))
		{
			event = push_output( "^^", &input->client, ClientOutput, context );
			if ( !event ) context->io.query.sync = 1;
		}
		else return output( Error, "> .. : no client input" );
	}
	else if ( !strcmp( state, "< cgi >: {_}_" ) ) {
		InputVA *input;
		if (( context->input.stack ) &&
		    (( input = context->input.stack->ptr )->mode == ClientInput ))
		{
			event = push_output( "^^", &input->client, ClientOutput, context );
			if ( !event ) {
				context->io.query.sync = 1;
				context->output.cgi = 1;
			}
		}
		else return output( Error, "< cgi >: no client input" );
	}
	return event;
}

int
clear_output_target( char *state, int event, char **next_state, _context *context )
{
	context->output.marked = 0;
	if ( !command_mode( 0, 0, ExecutionMode ) )
		return event;
	if ( !context->output.redirected || context->output.cgi )
		return event;
	OutputVA *output = context->output.stack->ptr;
	switch ( output->mode ) {
	case SessionOutput:
		// terminate both output and (after reading response
		// in case of io.query.sync) connection (IO_QUERY)
		pop_output( context, 1 );
		break;
	case FileOutput:
		pop_output( context, 0 );
		break;
	case ClientOutput:
		// terminate output, but not connection (IO_TICKET)
		// The latter will be closed upon transmission-closing
		// packet from client - cf. read_ & pop_input()
		pop_output( context, 0 );
		break;
	case StringOutput:
		pop_output( context, 0 );
		break;
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
			// genitor wants to sync on session's iam
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
	if ( !context->output.redirected ) {
		*next_state = ">:";
		return 0;
	}

	output( Text, "\n" );
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
	// save socket fd before output is popped
	int fd = output->ptr.socket;
	// terminate output, but not connection (IO_QUERY)
	// The latter will be closed upon pop_input( SessionOutputPipe... )
	pop_output( context, 0 );
	push_input( "", &fd, into, context );
	return 0;
}

int
pipe_output_in( char *state, int event, char **next_state, _context *context )
	{ return pipe_output( SessionOutputPipe, context ); }
int
pipe_output_out( char *state, int event, char **next_state, _context *context )
	{ return pipe_output( SessionOutputPipeOut, context ); }
int
pipe_output_to_variable( char *state, int event, char **next_state, _context *context )
	{ return pipe_output( SessionOutputPipeToVariable, context ); }

/*---------------------------------------------------------------------------
	scheme_op	- operator specific
---------------------------------------------------------------------------*/
static int create_session( char *path, _context *context );

int
scheme_op( char *state, int event, char **next_state, _context *context )
{
	char *scheme = context->identifier.scheme;
	if ( strcmp( scheme, "session" ) )
		return outputf( Error, "%s operation not supported", scheme );
	if ( context->command.op != InstantiateOperation )
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

static int
create_session( char *path, _context *context )
/*
	implements !! < session://path >	- operator only
*/
{
	SessionVA *session;
	// verify that a session with that name does not already exists
	registryEntry *entry = registryLookup( context->operator.sessions, path );
	if (( entry )) {
		session = entry->value;
		if ( session->pid )
			return outputf( Warning, "session '%s' already on - did not create", path );
	}
	else {
		// create session directory
		char *fullpath;
		asprintf( &fullpath, "%s/session/%s", CN_BASE_PATH, path );
		if ( mkdir( fullpath, 0777 ) && ( errno != EEXIST ) ) {
			free( fullpath );
			return outputf( Error, "create_session: %s", strerror(errno) );
		}
		free( fullpath );
		session = calloc( 1, sizeof(SessionVA) );
		session->path = path;
		session->operator = getpid();
	}
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
	cn_instf( "...", path, "is", "session" );
	registryRegister( context->operator.sessions, path, session );
	registryRegister( context->operator.pid, &session->pid, session );

	if (( context->input.stack )) {
		// transport session data in case genitor wants to sync
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
	char *pid = get_identifier( context, 2 );

	registryEntry *entry = registryLookup( context->operator.sessions, path );
	if ( entry == NULL )
		return outputf( Warning, "iam notification: path unknown: '%s'", path );

	SessionVA *session = entry->value;
	if ( session->pid != atoi( pid ) )
		return outputf( Warning, "iam event: unable to authenticate: '%s' - ignoring", path );

	if ( session->genitor ) {
		char *news;
		asprintf( &news, "< operator >: < session:\"%s\" > !!", path );

		int fd = session->genitor;
		int remainder = 0, length = strlen( news );
		io_write( fd, news, length, &remainder );
		io_finish( fd, &remainder, 0, context );
		// close genitor ticket - no sync
		write( fd, &remainder, sizeof( remainder ) );
		close( fd );

		session->genitor = 0;
		free( news );
	}
	return 0;
}

/*---------------------------------------------------------------------------
	handle_bye
---------------------------------------------------------------------------*/
int
handle_bye( char *state, int event, char **next_state, _context *context )
{
	if ( !context->control.operator )
		return output( Warning, "bye event: wrong address" );

	char *path = context->identifier.path;
	char *pid = get_identifier( context, 2 );

	registryEntry *entry = registryLookup( context->operator.sessions, path );
	if ( entry == NULL )
		return outputf( Warning, "bye notification: path unknown: '%s'", path );

	SessionVA *session = entry->value;
	if ( session->pid != atoi( pid ) )
		return outputf( Warning, "bye event: unable to authenticate: '%s' - ignoring", path );

	outputf( Warning, "operator: waving good-bye: %s=%d", path, session->pid );
	registryDeregister( context->operator.pid, &session->pid );
	session->pid = 0;
	return 0;
}

/*---------------------------------------------------------------------------
	input_cgi
---------------------------------------------------------------------------*/
static int
add_cgi_entry( char *state, int event, char **next_state, _context *context )
{
	Literal *literal = xl( context->expression.ptr );
	if (( literal )) {
		CNLog *log = &context->frame.log.cgi;
		addItem( &log->instantiated, literal );
		context->expression.ptr = NULL;
	}
	return 0;
}

int
input_cgi( char *state, int event, char **next_state, _context *context )
/*
	Assumption: cgi input is sent from cgi client, and followed by
	either 0 (pop client) or '|' (sync_delivery) or other..
	Essentially we do not support
		\< cgi >: { cgi inputs }
	as the last line of an HCN file - cf. command_pop_input()
*/
{
	if ( !command_mode( 0, 0, ExecutionMode ) )
		return 0;

	// build the log of cgi entries
	// ----------------------------
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
			on_( '\n' )	do_( add_cgi_entry, "< cgi >: {_}_" )
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
				on_( '\n' )	do_( nop, "< cgi >: {_}_" )
				on_other	do_( error, "" )
				end
		in_( "< cgi >: {_}_" ) bgn_
			on_( 0 )	do_( pop_input, "" )
			on_( '|' )	do_( set_output_target, "" )
			on_other	do_( nothing, "" )
			end
		end
	}
	while ( strcmp( state, "" ) );
	if ( event < 0 ) {
		CNLog *log = &context->frame.log.cgi;
		freeLiterals( &log->instantiated );
	}
	return event;
}
