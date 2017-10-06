#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <signal.h>

#include "database.h"
#include "registry.h"
#include "kernel.h"
#include "output.h"

#include "io.h"
#include "command.h"
#include "variables.h"

volatile sig_atomic_t terminating = 0;
static void
termination_handler( int signum )
{
/*
	Since this handler is established for more than one kind of signal, 
	it might still get invoked recursively by delivery of some other kind
	of signal.  Use a static variable to keep track of that.
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
	_context context;
	bzero( &context, sizeof(_context) );
	CN.context = &context;

	CN.nil = newEntity( NULL, NULL, NULL );
	CN.nil->sub[0] = CN.nil;
	CN.nil->sub[1] = CN.nil;
	CN.nil->sub[2] = CN.nil;
	CN.nil->state = 1;	// always on

	StackVA stack;
	bzero( &stack, sizeof(StackVA) );
	set_this_variable( &stack.variables, CN.this );

	registerByName( &CN.VB, "name", NULL );
	registerByName( &CN.VB, "hcn", NULL );
	registerByName( &CN.VB, "url", NULL );
	registerByName( &CN.VB, "narratives", NULL );

	context.hcn.state = "";
	context.control.mode = ExecutionMode;
	context.control.stack = newItem( &stack );
	context.control.terminal = isatty( STDIN_FILENO );
	context.control.prompt = context.control.terminal;

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

	io_init( &context );
	int event = read_command( base, 0, &same, &context );
	io_exit( &context );

	return event;
}

