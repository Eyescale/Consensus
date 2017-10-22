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

#define IO_OPERATOR_PATH	"/tmp/cn-operator.io"
#define IO_SERVICE_PATH		"/tmp/cn-service.%d.io"
#define IO_BULLETIN_PATH	"/tmp/cn-bulletin.%d.io"
#define IO_PEER_PATH		"/tmp/cn-service.%s.io"

#define IO_PORT	1
#define IO_CALL	2

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
	case IO_CALL:
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
	switch ( type ) {
	case IO_CALL:
#ifdef SYNC_CALL
		; int remainder = 0;
		char *buffer = CN.context->io.input.buffer.ptr;
		while ( io_read( socket_fd, buffer, &remainder ) )
			;
#endif
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

		// notify parent of existence
#ifdef DO_LATER
		cn_dof( ">@ operator: < session:%s >: %d @", context->session.identifier, getpid() );
			would require %.$( identifier ), %.$( pid )
#else
		int socket_fd = io_open( IO_CALL, IO_BULLETIN_PATH, getppid() );

		char *iam;
		asprintf( &iam, "< session:%s >: %d @", context->session.identifier, getpid() );

		int remainder = 0, length = strlen( iam );
		io_write( socket_fd, iam, length, &remainder );
		io_flush( socket_fd, &remainder, context );

		free( iam );
		io_close( IO_CALL, socket_fd, "" );
#endif
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

	if ( context->control.terminal &&  !context->control.anteprompt && !(context->input.stack) )
		context->control.prompt = 1;

	if (( context->input.stack ))
		return; // already at work

	// set I/O descriptors and call select()
	// -------------------------------------

	int nfds = 0;

	FD_SET( STDIN_FILENO, fds );
	nfds = SUP( nfds, STDIN_FILENO );

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
	cn_dof( ">@ operator: < session: %s @ %d >: %s", context->session.identifier, getpid(), change(s) );
	Q: why not inform subscribers directly ? Right now we leave the dispatching to the operator
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
int
io_accept( int fd, _context *context )
{
	struct sockaddr_un client_name;
	socklen_t client_name_len;

	int socket_fd = accept( fd, (struct sockaddr *) &client_name, &client_name_len );

	context->io.client = socket_fd;

	// read & execute command
	push( base, 0, NULL, context );
	cn_read( &socket_fd, ClientInput, base, 0 );
	pop( base, 0, NULL, context );

#ifdef SYNC_CALL
	// send closing packet
	int size = 0;
	write( socket_fd, &size, sizeof( size ));
#endif

	close( socket_fd );
	return 0;
}

/*---------------------------------------------------------------------------
	io_call
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
io_call( char *path, _context *context )
{
	if ( isanumber( path ) ) {
		if ( atoi( path ) == getpid() )
			return outputf( Error, ">@ %s: short-circuit", path );

		return io_open( IO_CALL, IO_PEER_PATH, path );
	}
	else if ( !strcmp( path, "operator" ) ) {
		if ( context->control.operator )
			return output( Error, ">@ operator: short-circuit" );

		if ( context->control.session )
			return io_open( IO_CALL, IO_SERVICE_PATH, getppid() );
		else
			return io_open( IO_CALL, IO_OPERATOR_PATH );
	}
	else	// target is a peer session
	{
		// query pid of the session associated with path from operator
#ifdef DO_LATER
		cn_dof( ">@ %%[ session:%s ].$( pid )", path );
		// the response should be in the variator variable
		return io_open( IO_CALL, IO_PEER_PATH, ??? );
#else
		int socket_fd;
		if ( context->control.session )
			socket_fd = io_open( IO_CALL, IO_SERVICE_PATH, getppid() );
		else
			socket_fd = io_open( IO_CALL, IO_OPERATOR_PATH );

		char *q;
		asprintf( &q, ">..:%%[ session:%s ].$( pid )", path );
		int remainder = 0, length = strlen( q );
		io_write( socket_fd, q, length, &remainder );
		io_flush( socket_fd, &remainder, context );

		// read the response

		IdentifierVA *pid = calloc( 1, sizeof(IdentifierVA) );
		string_start( pid, 0 );
		char *buffer = context->io.input.buffer.ptr;
		do {
			length = io_read( socket_fd, buffer, &remainder ) - 1;
			if ( !length ) {
				string_finish( pid, 0 );
				free( pid->ptr );
				free( pid );
				io_close( IO_CALL, socket_fd, "" );
				return outputf( Error, "session unknown: %s", path );
			}
			for ( int i=0; i<length; i++ )
				string_append( pid, buffer[ i ] );
		}
		while ( remainder );
		string_finish( pid, 1 );

		// wait for operator's closing packet
		while ( io_read( socket_fd, buffer, &remainder ) )
			;
		io_close( IO_CALL, socket_fd, "" );

		// set socket path according to response pid

		socket_fd = io_open( IO_CALL, IO_PEER_PATH, pid->ptr );
		free( pid->ptr );
		free( pid );
		return socket_fd;

#endif	// DO_LATER
	}
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

