#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "list.h"
#include "registry.h"
#include "database.h"
#include "kernel.h"

#include "command_util.h"
#include "input.h"
#include "input_util.h"
#include "output.h"
#include "path.h"
#include "narrative_util.h"
#include "string_util.h"

#include "api.h"
#include "frame.h"
#include "io.h"
#include "output_util.h"
#include "command.h"
#include "variable.h"
#include "variable_util.h"

#include "cgic.h"

// #define DEBUG
// #define CGI_DEBUG

/*---------------------------------------------------------------------------
        main
---------------------------------------------------------------------------*/
static void trap_signals( void );
static int clarg( int argc, char *argv[], char *value );
static char * clargv( int argc, char *argv[], char *option );
static _action command_init, command_exit;

int
main( int argc, char *argv[] )
{
	trap_signals();

	// initialize CN system data
	// --------------------------------------------------
	CN.nil = newEntity( NULL, NULL, NULL, 0 );
	CN.nil->sub[0] = CN.nil;
	CN.nil->sub[1] = CN.nil;
	CN.nil->sub[2] = CN.nil;
	CN.nil->state = 1;	// always on

	CN.VB = newRegistry( IndexedByName );

	registryRegister( CN.VB, "name", newRegistry(IndexedByAddress) );
	registryRegister( CN.VB, "hcn", newRegistry(IndexedByAddress) );
	registryRegister( CN.VB, "story", newRegistry(IndexedByAddress) );
	registryRegister( CN.VB, "url", newRegistry(IndexedByAddress) );
	registryRegister( CN.VB, "narratives", newRegistry(IndexedByAddress) );

	CN.names = newRegistry( IndexedByName );

	// initialize CN context data
	// --------------------------------------------------
	_context *context = CN.context = calloc( 1, sizeof(_context) );

	RTYPE( &context->command.stackbase.variables ) = IndexedByName;
	context->command.stackbase.next_state = "";
	context->command.stack = newItem( &context->command.stackbase );
	context->command.mode = ExecutionMode;
	context->frame.log.entities.backbuffer = &context->frame.log.entities.buffer[ 0 ];
	context->frame.log.entities.frontbuffer = &context->frame.log.entities.buffer[ 1 ];
	context->input.buffer.ptr = &context->input.buffer.base;
	context->hcn.state = "";
	context->control.terminal = isatty( STDIN_FILENO );
	context->error.tell = 1;
	context->control.cgi = (( argc == 1 ) && !context->control.terminal );
	context->control.cgim = clarg( argc, argv, "-mcgi" );
	context->control.operator = clarg( argc, argv, "-moperator" );
	struct stat buf;
	context->control.operator_absent = stat( IO_OPERATOR_PATH, &buf );
	if ( context->control.operator_absent ) {
		if ( context->control.operator ) {
			context->session.path = argv[ 0 ];
			context->operator.pid = newRegistry( IndexedByNumber );
			context->operator.sessions = newRegistry( IndexedByName );
			context->control.operator_absent = 0;
		}
		else if ( !context->control.cgi )
			output( Warning, "operator is out of order" );
	}
	else if ( context->control.operator )
		return outputf( Error, "operator already registered" );
	else if ( context->control.cgi || context->control.cgim )
		cn_new( strdup( "operator" ) );
	else {
		context->session.path = clargv( argc, argv, "-msession" );
		context->control.session = ( context->session.path != NULL );
	}
	context->output.prompt = ttyu( context );
		
	// go live
	// --------------------------------------------------
	char *state = base;
	int event = command_init( state, 0, &state, context );
	if ( event )
		; // e.g. cgiDebug()
	else if ( context->control.cgim || context->control.cgi ) {
		if ( context->control.cgim ) {
			// Let user set CGI entries
			context->error.tell = 1;
			do { event = read_command( state, event, &state, context );
			} while ( strcmp( state, "" ) );
		}
		if ( event >= 0 ) {
			event = 0;
			Narrative *n = cn_activate_narrative( CN.nil, "cgi" );
			printf( "Content-type: text/html\n\n" );
			do { event = frame_update( base, event, &same, context );
			} while ( narrative_active( CN.nil, n ) );
		}
	}
	else if ( context->control.session ) {
		Narrative *n = cn_activate_narrative( CN.nil, "story" );
		do {
			if ( event || ( io_scan( state, event, &same, context ) >= 0 ) )
				event = read_command( state, event, &state, context );
			event = frame_update( state, event, &same, context );
		}
		while ( narrative_active( CN.nil, n ) );
	}
	else	// operator and CLI
	{
		context->error.tell = 1; // must reset
		event = command_read( NULL, UserInput, base, event, context );
	}

	// exit gracefully
	// --------------------------------------------------
	return command_exit( state, event, &same, context );
}

static int
clarg( int argc, char *argv[], char *value )
{
	for ( int i=1; i<argc; i++ )
		if ( !strcmp( argv[ i ], value ) )
			return 1;
	return 0;
}
static char *
clargv( int argc, char *argv[], char *option )
{
	argc--;
	for ( int i=1; i<argc; )
		if ( !strcmp( argv[ i++ ], option ) )
			return argv[ i ];
	return NULL;
}

/*---------------------------------------------------------------------------
	command_init
---------------------------------------------------------------------------*/
static int cgiDebug( cgiFormEntry *entries );
static void restoreCNDB( _context * );

static int
command_init( char *state, int event, char **next_state, _context *context )
/*
	initialize CNDB and IO ports
*/
{
	if ( context->control.operator ) {
		char *cmd;
		asprintf( &cmd, "ls %s/session | sort", CN_BASE_PATH );
		push_input( "", cmd, UNIXPipeInput, context );
		StringVA *buffer = newString();
		do {
			event = read_input( base, 0, &same, context );
			switch ( event ) {
			case 0:
				pop_input( base, 0, &same, context );
				break;
			case '\n':
				; char *identifier = string_finish( buffer, 0 );
				buffer->value = NULL;
				if (( identifier )) {
					SessionVA *session = calloc( 1, sizeof(SessionVA) );
					session->path = identifier;
					session->operator = getpid();
					cn_instf( "...", identifier, "is", "session" );
					registryRegister( context->operator.sessions, identifier, session );
					outputf( Debug, "OPERATOR: Registered %s", identifier );
				}
				break;
			default:
				string_append( buffer, event );
			}
		}
		while ( event );
		freeString( buffer );
	}
	else if ( context->control.session ) {
		context->frame.updating = 1;
		cn_dof( ": session : \"%s\"", context->session.path );
		cn_do( ":< file://Session.story" );
		restoreCNDB( context );
		context->frame.updating = 0;
	}
	else if ( context->control.cgi || context->control.cgim ) {
		// load the cgi story
		context->frame.updating = 1;
		cn_do( ":< file://cgi.story" );
		context->frame.updating = 0;
		// instantiate cgi inputs
		if ( context->control.cgi ) {
			cgiInit();
			if ( cgiDebug( cgiFormEntryFirst ) ) return 1;
			for ( cgiFormEntry *i = cgiFormEntryFirst; i!=NULL; i=i->next ) {
				if ( ! *i->value )
					continue;
				if ( !strcmp( i->attr, "session" ) && isanumber( i->value ) )
					continue;
				cn_setf( "%s-is->%s", i->value, i->attr );
			}
			// cgiExit(); DO_LATER
		}
		/*
			in cgi emulation mode: only load the cgi story,
			as cgi entries will be created manually
			otherwise, create the cgi data name-value pairs
		*/
	}
	context->error.tell = 1; // must reset
	io_init( context );
	context->error.tell = 1; // must reset
	return 0;
}

extern int CGI_DEBUG_MARK;
static int
cgiDebug( cgiFormEntry *entries )
/*
        cgiDebug: utility - just output CGI entries
*/
{
#ifndef CGI_DEBUG
	return 0;
#endif
	if ( !CGI_DEBUG_MARK ) return 0;
	cgiFormEntry *found;
	for ( found = entries; found!=NULL; found=found->next )
		if ( !strcmp( found->attr, "entity" ) ) {
			char *s;
			for ( s=found->value; *s; s++ )
				if ( !strncmp( s, "%22", 3 ) )
					break;
			if (( *s )) break;
		}

	if (( found )) {
		printf( "Content-type: text/html\n\n" );
		printf( "< cgi >: " );
#if 1
		for ( cgiFormEntry *i = entries; i!=NULL; i=i->next )
			printf( "%s-is->%s, ", i->value, i->attr );
#else
		for ( cgiFormEntry *i = entries; i!=NULL; i=i->next )
			cn_setf( "%s-is->%s", i->value, i->attr );
		cn_do( ">:%[ .is. ]" );
#endif
		return 1;
	}
	else return 0;
}

static void
restoreCNDB( _context *context )
{
#if 1
	char *path = cn_path( context, "w", "//CNDB" );
	struct stat buf;
	if ( !stat( path, &buf ) ) {
		cn_do( ":< file://CNDB" );
	}
	free( path );
#else
	int level = context->input.level;
	if ( !push_input( "", strdup("//CNDB"), FileInput, context ) ) {
		do { event = read_command( state, event, &state, context );
		} while ( context->input.level > level );
	}
#endif
}

/*---------------------------------------------------------------------------
	command_exit
---------------------------------------------------------------------------*/
static void backupCNDB( _context * );

static int
command_exit( char *state, int event, char **next_state, _context *context )
{
	if ( context->control.operator ) {
		// exit all active sessions
		Registry *registry = context->operator.sessions;
		for ( registryEntry *r = registry->value; r!=NULL; r=r->next ) {
			SessionVA *session = r->value;
			if ( session->pid ) {
				cn_dof( ">@ %d:\\< cgi >: Leave-is->action |", session->pid );
			}
		}
	}
	else if ( context->control.session ) {
		// close ClientInput and ClientOutput.
		// These are both set to CGI (IO_TICKET) as the session story is
		// deactivated during the frame_update() triggered by CGI input
		clear_output_target( state, event, &state, context );
		while ( context->input.level > 0 )
			pop_input( base, 0, &same, context );

		backupCNDB( context );
	}
	context->error.tell = 1; // must reset
	io_exit( context );
	return 0;
}

static void
backupCNDB( _context *context )
{
	push_output( "", "//CNDB", FileOutput, context );
	char *path;
	if (( path = cn_va_get( CN.nil, "hcn" ) ))
		outputf( Text, ": $( hcn ) : \"%s\"\n", path );
	if (( path = cn_va_get( CN.nil, "story" ) ))
		outputf( Text, ": $( story ) : \"%s\"\n", path );

	for ( listItem *i = CN.DB; i!=NULL; i=i->next ) {
		Entity *e = i->ptr;
		if ((e->as_sub[ 0 ]) || (e->as_sub[ 1 ]) || (e->as_sub[ 2 ]))
			continue;
		output( Text, "!! " );
		output_entity( e, 3, NULL );
		output( Text, "\n" );
	}

	registryEntry *entry = registryLookup( CN.VB, "hcn" );
	Registry *registry = entry->value;
	for ( entry = registry->value; entry!=NULL; entry=entry->next ) {
		Entity *e = entry->index.address;
		if ( e == CN.nil ) continue;
		output( Text, ": $[ " );
		output_entity( e, 3, NULL );
		outputf( Text, " ]( hcn ) : \"%s\"\n", entry->value );
	}
	pop_output( context, 1 );
}

/*---------------------------------------------------------------------------
	signal handlers
---------------------------------------------------------------------------*/
static void termination_handler( int signum );
static void suspension_handler( int signum );

static void
trap_signals( void )
{
/*
	cause any child process that subsequently terminates
	to be immeditately removed from the system instead of
	being converted into a zombie
*/
	signal( SIGCHLD, SIG_IGN );
/*
	catch termination signals to call io_exit()
*/
	signal( SIGHUP, termination_handler );
	signal( SIGINT, termination_handler );
	signal( SIGKILL, termination_handler );
	signal( SIGPIPE, termination_handler );
	signal( SIGALRM, termination_handler );
	signal( SIGTERM, termination_handler );
	signal( SIGXCPU, termination_handler );
	signal( SIGXFSZ, termination_handler );
	signal( SIGVTALRM, termination_handler );
	signal( SIGPROF, termination_handler );
	signal( SIGUSR1, termination_handler );
	signal( SIGUSR2, termination_handler );
/*
	catch suspension signal - DO_LATER
*/
	signal( SIGTSTP, suspension_handler );
}

volatile sig_atomic_t terminating = 0;
static void
termination_handler( int signum )
{
/*
	Since this handler is established for more than one kind of signal, 
	it might still get invoked recursively by the delivery of some other kind
	of signal. Use a static variable to keep track of that.
	http://www.gnu.org/software/libc/manual/html_node/Termination-in-Handler.html#Termination-in-Handler
*/
	if ( terminating ) {
		raise( signum );
		return;
	}
	if ( signum == SIGPIPE )
		output( Debug, "SIGPIPE" );
	terminating = 1;
	io_exit( CN.context );
	signal( signum, SIG_DFL );
	raise( signum );
}

static void
suspension_handler( int signum )
{
	CN.context->control.stop = 1;
}

