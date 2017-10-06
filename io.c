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

#define SET_IO_SERVICE_PATH( name ) \
	sprintf( name, "/tmp/consensus.%d.io", getpid() );
#define SET_IO_SESSION_PATH( name, path ) \
        sprintf( name, "/tmp/consensus.%s.io", path );
#define GET_IO_SERVICE_PATH( name ) \
	asprintf( name, "/tmp/consensus.%d.io", getpid() );

/*---------------------------------------------------------------------------
	io_init		- called from main()
---------------------------------------------------------------------------*/
int
io_init( _context *context )
{
	// initialize buffers
	// ------------------
	context->io.input.buffer.ptr[ IO_BUFFER_MAX_SIZE - 1 ]	= '\0';
	context->io.output.buffer.ptr[ IO_BUFFER_MAX_SIZE - 1 ]	= '\0';
	context->io.pipe.buffer.ptr[ IO_BUFFER_MAX_SIZE - 1 ]	= '\0';
	
	// create io.service socket
	// ------------------------
	struct sockaddr_un name;
	int service_socket = socket( PF_LOCAL, SOCK_STREAM, 0 );

	// Indicate that this is a server
	name.sun_family = AF_LOCAL;
	SET_IO_SERVICE_PATH( name.sun_path );
	bind( service_socket, (struct sockaddr *) &name, SUN_LEN (&name) );

	context->io.service = service_socket;

	// Listen for service requests
	// ---------------------------
	listen( context->io.service, 5 );

	return 1;
}

/*---------------------------------------------------------------------------
	io_exit		- called from main()
---------------------------------------------------------------------------*/
int
io_exit( _context *context )
{
	char *socket_name;

	// close io.service socket
	// -----------------------
	GET_IO_SERVICE_PATH( &socket_name );
	close( context->io.service );
	unlink( socket_name );
	free( socket_name );

	return 1;
}

/*---------------------------------------------------------------------------
	io_connect
---------------------------------------------------------------------------*/
int
io_connect( ConnectionType type, char *path )
{
	// create the socket
	int socket_fd = socket( PF_LOCAL, SOCK_STREAM, 0 );

	// store the server’s name in the socket address
	struct sockaddr_un name;
	name.sun_family = AF_LOCAL;
        SET_IO_SESSION_PATH( name.sun_path, path );

	// connect the socket
	connect( socket_fd, (struct sockaddr *) &name, SUN_LEN(&name) );

	return socket_fd;
}

/*---------------------------------------------------------------------------
	io_read
---------------------------------------------------------------------------*/
int
io_read( int socket_fd, char *buffer, int *remainder )
{
	int length;
#ifdef DEBUG
	output( Debug, "io_read: fd=%d, remainder=%d", socket_fd, *remainder );
#endif
	// first read the size of the data to follow
	if ( ! *remainder ) {
		read( socket_fd, &length, sizeof(length) );
		if ( !length ) return 0;
	} else {
		length = *remainder;
	}

	// then read the data itself
	if ( length > IO_BUFFER_MAX_SIZE ) {
		int n = IO_BUFFER_MAX_SIZE - 1;
		read( socket_fd, buffer, n );
		*remainder = length - n;
		length = IO_BUFFER_MAX_SIZE;
	} else {
		*remainder = 0;
		read( socket_fd, buffer, length );
	}
#ifdef DEBUG
	output( Debug, "io_read: returning length: %d", length );
#endif
	return length;	// includes terminating null character
}

/*---------------------------------------------------------------------------
	io_write		- called from output()
---------------------------------------------------------------------------*/
int
io_write( int socket_fd, char *string, int length, int *remainder )
{
	char *buffer = CN.context->io.output.buffer.ptr;
	int size = *remainder;
	if ( !size )
	{
		size = 1;	// as in null-terminating character
	}
	else if ( size + length > IO_BUFFER_MAX_SIZE )
	{
		// send previous packet
		// --------------------
		write( socket_fd, &size, sizeof(size) );
		write( socket_fd, buffer, size );

		// start new packet
		// ----------------
		size = 1;	// as in null-terminating character
	}

	// break down data in sizeable chunks
	// ----------------------------------
	length++;
	int n = IO_BUFFER_MAX_SIZE - 1;
	for ( ; length >= IO_BUFFER_MAX_SIZE; length-=n, string+=n )
	{
		memcpy( buffer, string, n );
		buffer[ n++ ] = (char) 0;
		write( socket_fd, &n, sizeof(n) );
		write( socket_fd, buffer, n-- );
	}

	// expand latest packet from null-terminating character
	// ----------------------------------------------------
	size--;
	memcpy( &buffer[ size ], string, length );
	*remainder = size + length;

	return 0;
}

/*---------------------------------------------------------------------------
	io_flush		- called from pop_output()
---------------------------------------------------------------------------*/
int
io_flush( int socket_fd, int *remainder )
{
	char *buffer = CN.context->io.output.buffer.ptr;

	// flush previous packet
	// ---------------------
	int size = *remainder;
	if ( size ) {
		write( socket_fd, &size, sizeof(size) );
		write( socket_fd, buffer, size );
		*remainder = 0;
		size = 0;
	}

	// send closing packet
	// -------------------
	write( socket_fd, &size, sizeof( size ));

	return 0;
}

/*---------------------------------------------------------------------------
	io_notify_changes
---------------------------------------------------------------------------*/
static void
io_notify_changes( _context *context )
{
	FrameLog *log = &context->io.output.log;

	// notify changes to operator
#ifdef DO_LATER
#endif	// DO_LATER

	// free log
	freeListItem( &log->streams.instantiated );
	freeListItem( &log->entities.instantiated );
	freeListItem( &log->entities.released );
	freeListItem( &log->entities.activated );
	freeListItem( &log->entities.deactivated );
	freeRegistry( &log->narratives.activate );
	freeRegistry( &log->narratives.deactivate );
}

/*---------------------------------------------------------------------------
	io_accept
---------------------------------------------------------------------------*/
static int
io_accept( ConnectionType request, _context *context )
{
	struct sockaddr_un client_name;
	socklen_t client_name_len;
	int socket_fd = accept( context->io.service,
		(struct sockaddr *) &client_name, &client_name_len );

	context->io.client = socket_fd;

	// read & execute command
	push( base, 0, NULL, context );
	push_input( "", &socket_fd, ClientInput, context );
	int event = read_command( base, 0, &same, context );
	// input pops automatically upon closing packet
	pop( base, 0, NULL, context );

	// send closing packet
	int size = 0;
	write( socket_fd, &size, sizeof( size ));

	close( socket_fd );
	return 0;
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
		FD_SET( context->io.operator, &fds );
#endif
		FD_SET( context->io.service, &fds );
		nfds = SUP( STDIN_FILENO, SUP( context->io.service, context->io.operator )) + 1;

		// call select() - if not already at work
		// --------------------------------------
		int frame_isset = ( context->frame.backlog || test_log( context, OCCURRENCES ) );
#ifdef DEBUG
		output( Debug, "frame_isset: %d - backlog:%d, log:%d", frame_isset,
			context->frame.backlog, test_log( context, OCCURRENCES ));
#endif
		if ( context->input.stack != NULL )
			; // already at work
		else if ( frame_isset ) {
			if ( !test_log( context, OCCURRENCES ) )
				prompt( context );
			// here we poll - no prompt
			bzero( &timeout, sizeof( struct timeval ) );
			select( nfds, &fds, NULL, NULL, &timeout );
		} else {
			// here we block
#ifdef DEBUG
			output( Debug, "BEFORE select: " );
#endif
			prompt( context );
			select( nfds, &fds, NULL, NULL, NULL );
		}
		
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
			// we always count to three, so that external change
			// notifications can travel all the way to actions
			for ( int i=0; i<3; i++ ) {
				event = systemFrame( state, event, next_state, context );
				io_notify_changes( context );
				if ( !context->frame.backlog )
					break;
			}
		}

		// if not already working and no user input pending
		// ------------------------------------------------
		if (( context->input.stack == NULL ) && !FD_ISSET( STDIN_FILENO, &fds ))
		{
			// take one command
			if ( FD_ISSET( context->io.service, &fds ) ) {
				io_accept( ServiceRequest, context );
			}
		}
		else input_ready = 1;
	}
	while ( !input_ready );

	return ( event < 0 ) ? event : input( state, event, NULL, context );
}
