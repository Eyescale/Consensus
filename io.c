#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "database.h"
#include "registry.h"
#include "kernel.h"

#include "output.h"
#include "io.h"
#include "frame.h"
#include "input.h"
#include "command.h"

// #define DEBUG

/*---------------------------------------------------------------------------
	io_open		- called from main()
---------------------------------------------------------------------------*/
int
io_open( _context *context )
{
	struct sockaddr_un name;
	int client_sent_quit_message;

	// Create io.query socket
	context->io.query = socket( PF_LOCAL, SOCK_STREAM, 0 );

	// Indicate that this is a server
	name.sun_family = AF_LOCAL;
	sprintf( name.sun_path, "/tmp/consensus.%d.io", getpid() );
	bind( context->io.query, (struct sockaddr *) &name, SUN_LEN (&name) );

	// Listen for connections
	listen( context->io.query, 5 );
	return 1;
}

/*---------------------------------------------------------------------------
	io_close	- called from main()
---------------------------------------------------------------------------*/
int
io_close( _context *context )
{
	char *socket_name;

	asprintf( &socket_name, "/tmp/consensus.%d.io", getpid() );
	close( context->io.query );
	unlink( socket_name );

	free( socket_name );
	return 1;
}


/*---------------------------------------------------------------------------
	io_write		- called from output()
---------------------------------------------------------------------------*/
int
io_write( _context *context, OutputContentsType type, const char *format, va_list ap )
{
	switch ( type ) {
	case Text:
		break;
	default:
		return 0;	// DO_LATER: Error, Warning etc.
	}

	OutputVA *output = (OutputVA *) context->output.stack->ptr;

	char *str = NULL;
	vasprintf( &str, format, ap );
	if ( str == NULL ) return 0;

	int length = strlen( str );
	if ( !length ) { free( str ); return 0; }

	char *data = str;
	char *buffer = context->io.buffer.ptr[ 1 ];

	int size = context->io.buffer.size;
	if ( !size )
	{
		size = 1;	// as in null-terminating character
	}
	else if ( size + length > IO_BUFFER_MAX_SIZE )
	{
		// flush previous packet
		// ---------------------

		write( output->socket, &size, sizeof(size) );
		write( output->socket, buffer, size );

		// start new packet
		// ----------------

		size = 1;	// as in null-terminating character
	}

	// break down dataset in sizeable chunks
	// -------------------------------------

	length++;
	int n = IO_BUFFER_MAX_SIZE - 1;
	for ( ; length >= IO_BUFFER_MAX_SIZE; length-=n, data+=n )
	{
		memcpy( buffer, data, n );
		buffer[ n++ ] = (char) 0;
		write( output->socket, &n, sizeof(n) );
		write( output->socket, buffer, n-- );
	}

	// expand latest packet from null-terminating character
	// ----------------------------------------------------

	size--;
	memcpy( &buffer[ size ], data, length );
	context->io.buffer.size = size + length;

	free( str );
	return 0;
}

/*---------------------------------------------------------------------------
	io_flush		- called from pop_output()
---------------------------------------------------------------------------*/
int
io_flush( _context *context )
{
	OutputVA *output = (OutputVA *) context->output.stack->ptr;

	// flush previous packet
	// ---------------------

	int size = context->io.buffer.size;
	if ( size ) {
		write( output->socket, &size, sizeof(size) );
		write( output->socket, context->io.buffer.ptr[ 1 ], size );
		context->io.buffer.size = 0;
		size = 0;
	}

	// send closing packet
	// -------------------

	write( output->socket, &size, sizeof( size ));

	return 0;
}

/*---------------------------------------------------------------------------
	io_save_changes		- called from system_frame()
---------------------------------------------------------------------------*/
void
io_save_changes( _context *context )
{
	int i = context->io.frame.current;

	context->io.frame.instantiated[ i ] = context->frame.log.entities.instantiated;
	context->io.frame.released[ i ] = context->frame.log.entities.released;
	context->io.frame.activated[ i ] = context->frame.log.entities.activated;
	context->io.frame.deactivated[ i ] = context->frame.log.entities.deactivated;

	context->frame.log.entities.instantiated = NULL;
	context->frame.log.entities.released = NULL;
	context->frame.log.entities.activated = NULL;
	context->frame.log.entities.deactivated = NULL;
}

/*---------------------------------------------------------------------------
	io_notify_changes	- called from io_scan()
---------------------------------------------------------------------------*/
static void
io_notify_changes( _context *context )
{
	// notify changes to broker - every three frames

	for ( int i=0; i<3; i++ ) {
		freeListItem( &context->io.frame.instantiated[ i ] );
		freeListItem( &context->io.frame.released[ i ] );
		freeListItem( &context->io.frame.activated[ i ] );
		freeListItem( &context->io.frame.deactivated[ i ] );
	}
}

/*---------------------------------------------------------------------------
	io_scan		- called from read_command()
---------------------------------------------------------------------------*/
#define SUP( a, b ) ((a)>(b)?(a):(b))
int
io_scan( char *state, int event, char **next_state, _context *context )
{
	fd_set fds; int nfds = 1;
	struct timeval timeout;
	int input_ready = 0;

	if ( context_check( 0, 0, ExecutionMode ) && !strcmp( state, base ) &&
	   ( context->control.level == 0 ) && ( event == 0 ))
	do
	{
		FD_ZERO( &fds );
		FD_SET( STDIN_FILENO, &fds );
#ifdef DO_LATER
		FD_SET( context->io.broker, &fds );
#endif
		FD_SET( context->io.query, &fds );
		nfds = SUP( STDIN_FILENO, SUP( context->io.query, context->io.broker )) + 1;

		// call select() - if not already at work
		// --------------------------------------

		if ( !is_frame_log_empty( context ) )
			context->frame.backlog = 3;

		if ( context->input.stack != NULL )
			; // already at work
		else if ( context->frame.backlog )
		{
			bzero( &timeout, sizeof( struct timeval ) );
			select( nfds, &fds, NULL, NULL, &timeout );
		}
		else	// here we block
		{
#ifdef DEBUG
			fprintf( stderr, "BEFORE select: " );
#else
			prompt( context );
#endif
			select( nfds, &fds, NULL, NULL, NULL );
		}
		
		// if internal work to do or external change notifications
		// -------------------------------------------------------
#ifdef DO_LATER
		if ( FD_ISSET( context->io.broker, &fds ) ) {
			read and add inputs to frame log
			context->frame.backlog = 3;
		}
#endif

		if ( context->frame.backlog )
		{
			// we always count to three, so that external change
			// notifications can travel all the way to actions
			for ( int i=0; i<3 && context->frame.backlog; i++ ) {
				context->io.frame.current = i;
				event = systemFrame( state, event, next_state, context );
			}
			// send out internal change notifications
			io_notify_changes( context );
		}

		// if not already working and no user input, take one command
		// ----------------------------------------------------------

		if (( context->input.stack == NULL ) && !FD_ISSET( STDIN_FILENO, &fds ))
		{
			if ( FD_ISSET( context->io.query, &fds ) )
			{
				struct sockaddr_un client_name;
		                socklen_t client_name_len;
				int client_socket_fd;
	
				// accept connection
				client_socket_fd = accept( context->io.query,
					(struct sockaddr *) &client_name, &client_name_len );
#if 1
				// read { source, target, message }
				// log notification - will be processed in systemFrame()
#else
				push_output( NULL, &client_socket_fd, ClientOutput, context );

				// read & execute command
				push( base, 0, NULL, context );
				push_input( NULL, &client_socket_fd, ClientInput, context );
				int event = read_command( base, 0, &same, context );
				pop( base, 0, NULL, context );
	
				pop_output( context );
#endif
			}
		}
		else input_ready = 1;
	}
	while ( !input_ready );

	return ( event < 0 ) ? event : input( state, event, NULL, context );
}
