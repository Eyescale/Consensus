#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <xlocale.h>
#include <ctype.h>
#include <stdarg.h>

#include "database.h"
#include "registry.h"
#include "kernel.h"

#include "io.h"
#include "api.h"
#include "input_util.h"
#include "output.h"
#include "frame.h"
#include "input.h"
#include "command.h"

// #define DEBUG

/*---------------------------------------------------------------------------
	io_open, io_close
---------------------------------------------------------------------------*/
int
io_open( int type, char *format, ... )
{
	struct sockaddr_un socket_name;
	int socket_fd;
	socket_name.sun_family = AF_LOCAL;

	va_list ap;
	va_start( ap, format );
	vsprintf( socket_name.sun_path, format, ap );
	va_end( ap );

	socket_fd = socket( PF_LOCAL, SOCK_STREAM, 0 );
	switch ( type ) {
	case IO_QUERY:
		connect( socket_fd, (struct sockaddr *) &socket_name, SUN_LEN(&socket_name) );
		break;
	case IO_PORT:
		bind( socket_fd, (struct sockaddr *) &socket_name, SUN_LEN (&socket_name) );
		listen( socket_fd, 5 );
		break;
	default:
		output( Debug, "Error: io_open() - no type specified" );
		return -1;
	}
	return socket_fd;
}

void
io_close( int type, int socket_fd, char *format, ... )
{
	_context *context = CN.context;
	switch ( type ) {
	case IO_QUERY:
		if ( context->io.query.sync )
		{
			// read closing packet
			int remainder = 0;
			char *buffer = context->io.input.buffer.ptr;
			io_read( socket_fd, buffer, &remainder );
			context->io.query.sync = 0;
		}
		close( socket_fd );
		break;
	case IO_PORT:
		close( socket_fd );
		char *socket_path;
		va_list ap;
		va_start( ap, format );
		vasprintf( &socket_path, format, ap );
		va_end( ap );

		unlink( socket_path );
		free( socket_path );
	}
}

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
	
	// initialize change and service sockets
	// -------------------------------------

	if ( context->control.cgi || context->control.cgim )
		return 0;

	if ( context->control.operator )
	{
		context->io.bulletin = io_open( IO_PORT, IO_BULLETIN_PATH, getpid() );
		context->io.service = io_open( IO_PORT, IO_SERVICE_PATH, getpid() );
		context->io.cgiport = io_open( IO_PORT, IO_OPERATOR_PATH );
	}
	else if ( context->control.session )
	{
		context->io.bulletin = io_open( IO_PORT, IO_BULLETIN_PATH, getpid() );
		context->io.service = io_open( IO_PORT, IO_SERVICE_PATH, getpid() );

		// send iam notification
		cn_dof( ">@ operator:\\< session:%s >: %d @", context->session.path, getpid() );
	}
	else
	{
		context->io.service = io_open( IO_PORT, IO_SERVICE_PATH, getpid() );
		context->io.bulletin = io_open( IO_PORT, IO_BULLETIN_PATH, getpid() );
	}

	return 0;
}

/*---------------------------------------------------------------------------
	io_exit		- called from main()
---------------------------------------------------------------------------*/
int
io_exit( _context *context )
{
	if ( context->control.cgi || context->control.cgim )
		return 0;

	if ( context->control.operator ) {
		io_close( IO_PORT, context->io.bulletin, IO_BULLETIN_PATH, getpid() );
		io_close( IO_PORT, context->io.service, IO_SERVICE_PATH, getpid() );
		io_close( IO_PORT, context->io.cgiport, IO_OPERATOR_PATH );
	} else {
		io_close( IO_PORT, context->io.bulletin, IO_BULLETIN_PATH, getpid() );
		io_close( IO_PORT, context->io.service, IO_SERVICE_PATH, getpid() );
	}

	return 0;
}

/*---------------------------------------------------------------------------
	io_scan		- called from read_command()
---------------------------------------------------------------------------*/
#define SUP( a, b ) ((a)>(b)?(a):(b))
void
io_scan( fd_set *fds, int blocking, _context *context )
{
	FD_ZERO( fds );

	if ( context->control.cgi || context->control.cgim )
		return;	// CGI does not listen to anyone

	if ( ttyu( context ) &&  !context->control.anteprompt && !(context->input.stack) )
		context->control.prompt = 1;

	if (( context->input.stack ))
		return; // already at work

	// set I/O descriptors and call select()
	// -------------------------------------

	int nfds = 0;

	if ( !context->control.session )
	{
		FD_SET( STDIN_FILENO, fds );
		nfds = SUP( nfds, STDIN_FILENO );
	}

	FD_SET( context->io.bulletin, fds );
	nfds = SUP( nfds, context->io.bulletin );

	FD_SET( context->io.service, fds );
	nfds = SUP( nfds, context->io.service );

	if ( context->control.operator )
	{
		FD_SET( context->io.cgiport, fds );
		nfds = SUP( nfds, context->io.cgiport );

	}

	nfds++;

	if ( blocking ) {
		prompt( context );
		select( nfds, fds, NULL, NULL, NULL );
	} else {
		struct timeval timeout;
		bzero( &timeout, sizeof( struct timeval ) );
		if ( !test_log( context, OCCURRENCES ) )
			prompt( context );
		select( nfds, fds, NULL, NULL, &timeout );
	}
}

/*---------------------------------------------------------------------------
	io_inform
---------------------------------------------------------------------------*/
void
io_inform( _context *context )
{
	FrameLog *log = &context->io.output.log;

	// notify changes to operator
#ifdef DO_LATER
	cn_dof( ">@ operator:\\< session: %s @ %d >: %s", context->session.path, getpid(), change(s) );
	Q: why not inform subscribers directly ? Right now we leave the dispatching to the operator
#endif	// DO_LATER

	// free log
	freeListItem( &log->streams.instantiated );
	freeListItem( &log->entities.instantiated );
	freeListItem( &log->entities.released );
	freeListItem( &log->entities.activated );
	freeListItem( &log->entities.deactivated );
	emptyRegistry( &log->narratives.activate );
	emptyRegistry( &log->narratives.deactivate );
}

/*---------------------------------------------------------------------------
	io_accept
---------------------------------------------------------------------------*/
int
io_accept( int fd, _context *context )
{
	struct { int sync; } backup;
	backup.sync = context->io.query.sync;
	context->io.query.sync = 0;

	struct sockaddr_un client_name;
	socklen_t client_name_len;
	int socket_fd = accept( fd, (struct sockaddr *) &client_name, &client_name_len );

	context->io.client = socket_fd;

	// read & execute command
	push( base, 0, NULL, context );
	cn_read( &socket_fd, ClientInput, base, 0 );
	pop( base, 0, NULL, context );

	// send closing packet if required
	if ( context->io.query.sync ) {
		int size = 0;
		write( socket_fd, &size, sizeof( size ));
	}
	if ( context->io.query.leave_open ) {
		context->io.query.leave_open = 0;
	}
	else close( socket_fd );


	context->io.query.sync = backup.sync;
	return 0;
}

/*---------------------------------------------------------------------------
	io_query
---------------------------------------------------------------------------*/
static int
isanumber( char *string )
{
	int len = strlen( string );
	for ( int i=0; i<len; i++ )
		if ( !isdigit( string[i] ) )
			return 0;
	return 1; 
}

int
io_query( char *path, _context *context )
{
	if ( isanumber( path ) ) {
		if ( atoi( path ) == getpid() )
			return outputf( Error, ">@ %s: short-circuit", path );

		return io_open( IO_QUERY, IO_PEER_PATH, path );
	}
	else if ( context->control.operator_absent )
			;
	else if ( !strcmp( path, "operator" ) ) {
		if ( context->control.operator )
			return output( Error, ">@ operator: short-circuit" );

		if ( context->control.session )
			return io_open( IO_QUERY, IO_SERVICE_PATH, getppid() );
		else
			return io_open( IO_QUERY, IO_OPERATOR_PATH );
	}
	else	// target is a peer session
	{
		// query operator the session descriptor associated with path
#ifdef DEBUG
		output( Debug, "io_query: contacting operator" );
		cn_dof( ">@ operator:< \\%%[ session:%s ]", path );
		output( Debug, "io_query: operator over" );
#else
		int retval = cn_dof( ">@ operator:< \\%%[ session:%s ]", path );
#endif
		if (( context->io.query.results )) {
			char *pid = context->io.query.results->ptr;
			if ( atoi( pid ) == getpid() )
				return outputf( Error, ">@ %s: short-circuit", path );

			return io_open( IO_QUERY, IO_PEER_PATH, pid );
		}
	}
	outputf( Error, "'%s': could not connect", path );
	return 0;
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
	io_flush
---------------------------------------------------------------------------*/
int
io_flush( int socket_fd, int *remainder, _context *context )
{
	char *buffer = context->io.output.buffer.ptr;

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

