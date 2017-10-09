#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/select.h>

#include "database.h"
#include "registry.h"
#include "kernel.h"
#include "output.h"

#include "frame.h"
#include "io.h"
#include "command.h"
#include "variables.h"

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
	main
---------------------------------------------------------------------------*/
int
main( int argc, char ** argv )
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

	// initialize CN data
	// ------------------

	CN.nil = newEntity( NULL, NULL, NULL );
	CN.nil->sub[0] = CN.nil;
	CN.nil->sub[1] = CN.nil;
	CN.nil->sub[2] = CN.nil;
	CN.nil->state = 1;	// always on

	registerByName( &CN.VB, "name", NULL );
	registerByName( &CN.VB, "hcn", NULL );
	registerByName( &CN.VB, "url", NULL );
	registerByName( &CN.VB, "narratives", NULL );

	CN.context = calloc( 1, sizeof(_context) );

	// initialize context data
	// -----------------------

#ifdef DUP_TEST
	int fd = open( "toto", O_RDONLY );
	close( STDIN_FILENO );
	dup( fd );
#endif

	_context *context = CN.context;

	context->control.stack = newItem( calloc(1,sizeof(StackVA)) );
	context->hcn.state = "";
	context->control.mode = ExecutionMode;
	context->control.terminal = isatty( STDIN_FILENO );
	context->control.prompt = context->control.terminal;

	io_init( context );

	// go live
	// -------

	int event = 0;
	char *state = base;
	char *next_state = same;
	do {
		if (( event == 0 ) && !strcmp( state, base ) && context_check( 0, 0, ExecutionMode ) &&
		    ( context->control.level == 0 ))	// DO_LATER: replace this last condition
		{
			// scan internal and external sources
			// ----------------------------------
			fd_set fds;
			int frame_isset = ( context->frame.backlog || test_log( context, OCCURRENCES ) );
#ifdef DEBUG
			output( Debug, "frame_isset: %d - backlog:%d, log:%d", frame_isset,
				context->frame.backlog, test_log( context, OCCURRENCES ));
#endif
			io_scan( &fds, !frame_isset, context );

			// if external change notifications
			// --------------------------------
#ifdef DO_LATER
			if ( FD_ISSET( context->io.operator, &fds ) ) {
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
					io_notify_changes( context );
					if ( !context->frame.backlog )
						break;	// optimization
				}
			}

			// if stdin or other input pending
			// -------------------------------
			if ( FD_ISSET( STDIN_FILENO, &fds ) || (context->input.stack) )
			{
				event = read_command( state, event, &next_state, context );
				state = next_state;
			}
			// otherwise if service request
			// ----------------------------
			else if ( FD_ISSET( context->io.service, &fds ) )
			{
				// take one command
				io_accept( ServiceRequest, context );
			}
		}
		else	// when life is simple...
		{
			event = read_command( state, event, &next_state, context );
			state = next_state;
		}
	}
	while ( strcmp( state, "" ) );

	io_exit( context );

	return event;
}

