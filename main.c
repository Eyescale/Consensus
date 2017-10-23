#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <fcntl.h>

#include "database.h"
#include "registry.h"
#include "kernel.h"
#include "output.h"
#include "narrative.h"

#include "api.h"
#include "frame.h"
#include "io.h"
#include "command.h"
#include "variables.h"

#include "cgic.h"

#define CN_CGI_STORY_PATH \
	"/Library/WebServer/Documents/consensus/cgi.story"
#define CN_HCN_HOME_PATH \
	"/Library/WebServer/Documents/consensus/hcn/Home.hcn"

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

	registerByName( &CN.VB, "name", NULL );
	registerByName( &CN.VB, "hcn", NULL );
	registerByName( &CN.VB, "url", NULL );
	registerByName( &CN.VB, "narratives", NULL );

	// initialize context data
	// -----------------------

	CN.context = calloc( 1, sizeof(_context) );

	_context *context = CN.context;

	StackVA *stack = calloc( 1, sizeof( StackVA ) );
	stack->next_state = "";
	context->control.stack = newItem( stack );
	context->hcn.state = "";
	context->control.mode = ExecutionMode;
	context->control.terminal = isatty( STDIN_FILENO );
	context->control.cgi = (( argc == 1 ) && !context->control.terminal );
	context->control.cgim = clarg( argc, argv, "-mcgi" );
	context->control.prompt = ttyu( context );
	context->control.operator = clarg( argc, argv, "-moperator" );
	context->session.path = clargv( argc, argv, "-msession" );
	context->control.session = ((context->session.path) ? 1 : 0);

	// initialize CNDB data
	// --------------------

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
		{
			Entity *entry = cn_setf( "%s-nvp->%s", i->attr, i->value );
			cn_setf( "%s-is->[ name<-has-[ entry<-is-%e ] ]", i->attr, entry );
			cn_setf( "%s-is->[ value<-has-[ entry<-is-%e ] ]", i->value, entry );
		}
		// 2. load and activate the cgi story
		cn_dof( ":< file:%s", CN_CGI_STORY_PATH );
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

	// go live
	// -------

	io_init( context );

	char *state = base;
	char *next_state = same;
	int restore[ 3 ] = { 0, 0, 0 };
	int event = command_init( NULL, UserInput, restore, context );
	do {
		if (( event == 0 ) && !strcmp( state, base ) && context_check( 0, 0, ExecutionMode ) &&
		    ( context->control.level == 0 ))	// DO_LATER: replace this last condition
		{
			// scan internal and external sensors
			// ----------------------------------
			fd_set fds;
			int frame_isset = ( context->frame.backlog || test_log( context, OCCURRENCES ) );
#ifdef DEBUG
			outputf( Text, "frame_isset: %d - backlog:%d, log:%d<BR>", frame_isset,
				context->frame.backlog, test_log( context, OCCURRENCES ));
#endif
			io_scan( &fds, !frame_isset, context );

			// if external change notifications
			// --------------------------------
#ifdef DO_LATER
			if ( FD_ISSET( context->io.bulletin, &fds ) ) {
				read and add inputs to frame log
				frame_isset = 1;
			}
#endif
			// if internal work to do
			// ----------------------
			if ( frame_isset )
			{
				// make it triple, so that occurrences and events can travel
				// all the way to actions - and those executed
				for ( int i=0; i<3; i++ ) {
					event = systemFrame( state, event, NULL, context );
					// notify outside observers
					io_inform( context );
					if ( !context->frame.backlog )
						break;	// optimization
				}
			}

			// if stdin or other internal input pending
			// ----------------------------------------
			if ( FD_ISSET( STDIN_FILENO, &fds ) || (context->input.stack) )
			{
				event = read_command( state, event, &next_state, context );
				state = next_state;
			}
			// otherwise if cgi request
			// ------------------------
			else if ( context->control.operator && FD_ISSET( context->io.cgiport, &fds ) )
			{
				// take one command
				io_accept( context->io.cgiport, context );
			}
			// otherwise if service request
			// ----------------------------
			else if ( FD_ISSET( context->io.service, &fds ) )
			{
				// take one command
				io_accept( context->io.service, context );
			}
		}
		else	// when life is simple...
		{
			event = read_command( state, event, &next_state, context );
			state = next_state;
		}
		if (( context->control.cgi || context->control.cgim ) && !narrative_active( context ))
			state = "";
	}
	while ( strcmp( state, "" ) );

	command_exit( state, restore, context );
	io_exit( context );
	// cgiExit();
	return event;
}

