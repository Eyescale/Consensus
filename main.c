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

#include "database.h"
#include "registry.h"
#include "kernel.h"
#include "output.h"
#include "narrative_util.h"

#include "api.h"
#include "frame.h"
#include "io.h"
#include "command.h"
#include "variable.h"
#include "variable_util.h"

#include "cgic.h"

// #define DEBUG

/*---------------------------------------------------------------------------
	signal handlers
---------------------------------------------------------------------------*/
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

/*---------------------------------------------------------------------------
	clarg, clargv
---------------------------------------------------------------------------*/
int
clarg( int argc, char *argv[], char *value )
{
	for ( int i=1; i<argc; i++ )
		if ( !strcmp( argv[ i ], value ) )
			return 1;
	return 0;
}
char *
clargv( int argc, char *argv[], char *option )
{
	argc--;
	for ( int i=1; i<argc; )
		if ( !strcmp( argv[ i++ ], option ) )
			return argv[ i ];
	return NULL;
}

/*---------------------------------------------------------------------------
        main
---------------------------------------------------------------------------*/
int
main( int argc, char *argv[] )
{
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

	signal( SIGTSTP, suspension_handler );

	// initialize system data
	// ----------------------

	CN.nil = newEntity( NULL, NULL, NULL );
	CN.nil->sub[0] = CN.nil;
	CN.nil->sub[1] = CN.nil;
	CN.nil->sub[2] = CN.nil;
	CN.nil->state = 1;	// always on

	CN.VB = newRegistry( IndexedByName );

	registryRegister( CN.VB, "name", newRegistry(IndexedByAddress) );
	registryRegister( CN.VB, "hcn", newRegistry(IndexedByAddress) );
	registryRegister( CN.VB, "url", newRegistry(IndexedByAddress) );
	registryRegister( CN.VB, "narratives", newRegistry(IndexedByAddress) );

	CN.names = newRegistry( IndexedByName );

	// initialize context data
	// -----------------------

	CN.context = calloc( 1, sizeof(_context) );

	_context *context = CN.context;

	RTYPE( &context->command.stackbase.variables ) = IndexedByName;
	context->command.stackbase.next_state = "";
	context->command.stack = newItem( &context->command.stackbase );
	context->command.mode = ExecutionMode;
	context->frame.log.entities.backbuffer = &context->frame.log.entities.buffer[ 0 ];
	context->frame.log.entities.frontbuffer = &context->frame.log.entities.buffer[ 1 ];
	context->input.buffer.current = &context->input.buffer.base;
	context->hcn.state = "";
	context->control.terminal = isatty( STDIN_FILENO );
	context->control.cgi = (( argc == 1 ) && !context->control.terminal );
	context->control.cgim = clarg( argc, argv, "-mcgi" );
	context->control.operator = clarg( argc, argv, "-moperator" );
	struct stat buf;
	context->control.operator_absent = stat( IO_OPERATOR_PATH, &buf );
	if ( context->control.operator ) {
		if ( !context->control.operator_absent )
			return outputf( Error, "operator already registered" );
		context->session.path = argv[ 0 ];
		context->operator.pid = newRegistry( IndexedByNumber );
		context->operator.sessions = newRegistry( IndexedByName );
	} else {
		if ( context->control.operator_absent )
			output( Warning, "operator is out of order" );
		context->session.path = clargv( argc, argv, "-msession" );
		context->control.session = ((context->session.path) ? 1 : 0);
	}
	context->output.prompt = ttyu( context );

	// initialize CNDB and IO data
	// ---------------------------

	if ( context->control.cgi || context->control.cgim )
	{
		cgiInit();
		if ( context->control.cgim ) {
			cgiFormEntry *first = calloc( 1, sizeof(cgiFormEntry) );
			first->attr = strdup( "session" );
			first->value = strdup( "join" );
			cgiFormEntryFirst = first;
		}
		printf( "Content-type: text/html\n\n" );

		// 1. create the cgi data name-value pairs
		for ( cgiFormEntry *i = cgiFormEntryFirst; i!=NULL; i=i->next )
			cn_setf( "%s-is->%s", i->value, i->attr );

		// 2. load and activate the cgi story
		context->frame.updating = 1;
		cn_dof( ":< file:%s", CN_CGI_STORY_PATH );
		context->frame.updating = 0;
		cn_activate_narrative( CN.nil, "cgi" );
	}
	else if ( !context->control.operator )
	{
		char *hcn = clargv( argc, argv, "-mhcn" );
		if ((hcn)) cn_dof( ": %%.$( hcn ) : \"%s\"", hcn );
		else cn_dof( ": %%.$( hcn ) : \"%s\"", CN_HCN_HOME_PATH );
#ifdef DO_LATER
		char *story = clargv( argc, argv, "-mstory" );
		if ((story)) cn_dof( ":< file:%s", story );
#endif
	}

	io_init( context );

	// go live
	// -------

	int event = 0;
	if ( context->control.cgi || context->control.cgim )
	{
		do { event = frame_update( base, event, &same, context ); }
		while ( narrative_active( context ) );
	}
	else
	{
		event = command_read( NULL, UserInput, base, event, context );
	}

	// exit gracefully
	// ---------------

	io_exit( context );
#ifdef DO_LATER
	cgiExit();	// does not exist yet
#endif
	return event;
}

