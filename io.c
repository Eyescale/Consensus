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
io_open( int type, void *fmt, ... )
{
	_context *context = CN.context;
	struct sockaddr_un socket_name;
	int socket_fd;

	if ( type != IO_TICKET ) {
		socket_name.sun_family = AF_LOCAL;
		va_list ap;
		va_start( ap, (char *) fmt );
		vsprintf( socket_name.sun_path, (char *) fmt, ap );
		va_end( ap );
		socket_fd = socket( PF_LOCAL, SOCK_STREAM, 0 );
	}

	switch ( type ) {
	case IO_TICKET:
		context->io.restore.sync = context->io.query.sync;
		context->io.query.sync = 0;
		socklen_t socket_name_len;
		socket_fd = accept( *( int *) fmt, (struct sockaddr *) &socket_name, &socket_name_len );
		break;
	case IO_QUERY:
		connect( socket_fd, (struct sockaddr *) &socket_name, SUN_LEN(&socket_name) );
		break;
	case IO_PORT:
		bind( socket_fd, (struct sockaddr *) &socket_name, SUN_LEN (&socket_name) );
		listen( socket_fd, 5 );
		break;
	}
	return socket_fd;
}

void
io_close( int type, int socket_fd, char *format, ... )
{
	_context *context = CN.context;
	switch ( type ) {
	case IO_TICKET:
		if ( context->io.query.leave_open ) {
			context->io.query.leave_open = 0;
			break;
		}
		// send closing packet if required
		if ( context->io.query.sync ) {
			int size = 0;
			write( socket_fd, &size, sizeof( size ));
		}
		close( socket_fd );
		context->io.query.sync = context->io.restore.sync;
		break;
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
	
	// initialize bulletin and service sockets
	// ---------------------------------------

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
int
io_scan( char *state, int event, char **next_state, _context *context )
{
	if ( !command_mode( 0, 0, ExecutionMode ) )
		return event;
	if ( event || strcmp( state, base ) )
		return event;
	if (( context->input.stack ))
		return event;
	if ( context->control.cgi )
		return event;	// CGI does not listen to anyone

	// set I/O descriptors
	// -------------------
	fd_set fds;
	FD_ZERO( &fds );

	int nfds = 0;

	if ( !context->control.session ) {
		FD_SET( STDIN_FILENO, &fds );
		nfds = SUP( nfds, STDIN_FILENO );
	}

	if ( !context->control.cgim ) {
		FD_SET( context->io.bulletin, &fds );
		nfds = SUP( nfds, context->io.bulletin );

		FD_SET( context->io.service, &fds );
		nfds = SUP( nfds, context->io.service );
	}

	if ( context->control.operator ) {
		FD_SET( context->io.cgiport, &fds );
		nfds = SUP( nfds, context->io.cgiport );
	}

	nfds++;

	// call select
	// -----------
	context->output.prompt |= ( ttyu( context ) && !context->output.anteprompt );
	int occurrence_pending = test_log( context, OCCURRENCES );

	if ( context->frame.backlog || occurrence_pending )
	{
		struct timeval timeout;
		bzero( &timeout, sizeof( struct timeval ) );
		if ( !occurrence_pending ) prompt( context );
		select( nfds, &fds, NULL, NULL, &timeout );
	}
	else	// blocking
	{
		prompt( context );
		select( nfds, &fds, NULL, NULL, NULL );
	}

	// override input channel if required
	// ----------------------------------
	if ( FD_ISSET( STDIN_FILENO, &fds ) )
		;
	else if ( context->control.operator && FD_ISSET( context->io.cgiport, &fds ) )
		push_input( "", &context->io.cgiport, ClientInput, context );
	else if ( FD_ISSET( context->io.service, &fds ) )
		push_input( "", &context->io.service, ClientInput, context );

	return 0;
}

/*---------------------------------------------------------------------------
	io_notify
---------------------------------------------------------------------------*/
void
io_notify( EntityLog *log, _context *context )
{
	// notify changes to operator
#ifdef DO_LATER
	cn_dof( ">@ operator:\\< session:%s @ %d >: %s", context->session.path, getpid(), change(s) );
	Q: why not inform subscribers directly ? Right now we leave the dispatching to the operator
#endif	// DO_LATER
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
		if ( atoi( path ) == getpid() ) {
			return outputf( Error, ">@ %s: short-circuit", path );
		}
		return io_open( IO_QUERY, IO_PEER_PATH, path );
	}
	else if ( context->control.operator_absent )
	{
		return output( Error, "1: operator out of order" );
	}
	else if ( !strcmp( path, "operator" ) ) {
		if ( context->control.operator ) {
			return output( Error, ">@ operator: short-circuit" );
		}
		if ( context->control.session )
			return io_open( IO_QUERY, IO_SERVICE_PATH, getppid() );
		else
			return io_open( IO_QUERY, IO_OPERATOR_PATH );
	}
	else	// target is a peer session
	{
		// query operator the session descriptor associated with path
		int retval = cn_dof( ">@ operator:< \\%%[ session:%s ]", path );

		if (( context->io.query.results )) {
			char *pid = context->io.query.results->ptr;
			if ( atoi( pid ) == getpid() ) {
				return outputf( Error, ">@ %s: short-circuit", path );
			}
			return io_open( IO_QUERY, IO_PEER_PATH, pid );
		}
	}
	return outputf( Error, "'%s': session unknown", path );
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
io_flush( int socket_fd, int *remainder, int eot, _context *context )
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

	// send data closing packet
	// ------------------------
	write( socket_fd, &size, sizeof( size ));

	// end transmission if required
	// ----------------------------
	if ( !eot ) return 0;

	if ( context->io.query.sync )
	{
		// free previous query results
		listItem **results = &context->io.query.results;
		for ( listItem *i = *results; i!=NULL; i=i->next )
			free( i->ptr );
		freeListItem( results );

		// read new results, including result terminating packet
		IdentifierVA dst = { NULL, NULL };
		int remainder = 0;
		char *buffer = context->io.input.buffer.ptr;
		while ( io_read( socket_fd, buffer, &remainder ) )
			for ( char *ptr = buffer; *ptr; ptr++ )
				slist_append( results, &dst, *ptr, 1, 0 );
		slist_close( results, &dst, 0 );
#ifdef DEBUG
		if (( *results ))
			for ( listItem *i = *results; i!=NULL; i=i->next )
				outputf( Debug, "io_flush: received: %s", (char *) i->ptr );
#endif
	}
	io_close( IO_QUERY, socket_fd, "" );

	return 0;
}

