#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include "database.h"
#include "registry.h"
#include "kernel.h"

#include "input.h"
#include "input_util.h"
#include "hcn.h"

// #define DEBUG

/*---------------------------------------------------------------------------
	engine
---------------------------------------------------------------------------*/
#define do_( a, s ) \
	event = hcn_execute( a, &state, event, s, context );

static _action hcn_CR;
static _action hcn_finish;

static int
hcn_execute( _action action, char **state, int event, char *next_state, _context *context )
{
	event = action( *state, event, &next_state, context );

	if ( ( action == hcn_CR ) ||
	     ( action == hcn_finish ) )
	{
		context->hcn.state = strcmp( next_state, same ) ? next_state : *state;
		*state = "";
	}
	else if ( strcmp( next_state, same ) ) {
		*state = next_state;
	}

	return event;
}

/*---------------------------------------------------------------------------
	actions
---------------------------------------------------------------------------*/
static int
hcn_start_out( char *state, int event, char **next_state, _context *context )
{
	string_start( &context->hcn.buffer, '>' );
	string_append( &context->hcn.buffer, ':' );
	if ( event != '%' ) {
		string_append( &context->hcn.buffer, '\\' );
	}
	string_append( &context->hcn.buffer, event );
	return 0;
}
static int
hcn_CR( char *state, int event, char **next_state, _context *context )
{
	hcn_start_out( state, '\n', next_state, context );
	string_finish( &context->hcn.buffer, 0 );
	return 0;
}
static int
hcn_start( char *state, int event, char **next_state, _context *context )
{
	string_start( &context->hcn.buffer, event );
	return 0;
}
static int
hcn_start_LT( char *state, int event, char **next_state, _context *context )
{
	hcn_start_out( state, '<', next_state, context );
	string_append( &context->hcn.buffer, event );
	return 0;
}
static int
hcn_start_GT( char *state, int event, char **next_state, _context *context )
{
	hcn_start_out( state, '>', next_state, context );
	string_append( &context->hcn.buffer, event );
	return 0;
}
static int
hcn_append( char *state, int event, char **next_state, _context *context )
{
	string_append( &context->hcn.buffer, event );
	return 0;
}
static int
hcn_append_GT( char *state, int event, char **next_state, _context *context )
{
	string_append( &context->hcn.buffer, '>' );
	string_append( &context->hcn.buffer, event );
	return 0;
}
static int
hcn_append_LT( char *state, int event, char **next_state, _context *context )
{
	string_append( &context->hcn.buffer, '<' );
	string_append( &context->hcn.buffer, event );
	return 0;
}
static int
hcn_finish( char *state, int event, char **next_state, _context *context )
{
	string_append( &context->hcn.buffer, '\n' );
	string_finish( &context->hcn.buffer, 1 );
	return 0;
}
static int
hcn_clear( char *state, int event, char **next_state, _context *context )
{
	free( context->hcn.buffer.ptr );
	freeListItem( &context->hcn.buffer.list );
	return 0;
}

/*---------------------------------------------------------------------------
	hcn_getc
---------------------------------------------------------------------------*/
int
hcn_getc( int fd, _context *context )
{
	int event = 0;

#ifdef DEBUG
	char *state = context->hcn.state;
	output( Debug, "entering hcn_getc: \"%s\"", state );
#endif

	do {
		char *state = context->hcn.state;

		// flush buffer
		// ------------
		if ( context->hcn.buffer.ptr != NULL )
		do {
			if ( context->hcn.position == NULL ) {
				context->hcn.position = context->hcn.buffer.ptr;
#ifdef DEBUG
				output( Debug, "hcn: reading from: \"%s\"", (char *) context->hcn.position );
#endif
			}
			event = (int ) (( char *) context->hcn.position++ )[ 0 ];
			if ( !event ) {
				free( context->hcn.buffer.ptr );
				context->hcn.buffer.ptr = NULL;
				context->hcn.position = NULL;
			}
			else {
				return event;
			}
		}
		while ( context->hcn.buffer.ptr != NULL );

		// input new data
		// --------------
		do {
			if ( !event ) read( fd, &event, 1 );
#ifdef DEBUG
			output( Debug, "hcn_getc: \"%s\", '%c'", state, event );
#endif
			bgn_
			on_( 0 ) return 0;
			in_( base ) bgn_
				on_( '<' )	do_( nop, "<" )
				on_( '\n' )	do_( hcn_CR, base )
				on_other	do_( hcn_start_out, ">:_" )
				end
				in_( "<" ) bgn_
					on_( '<' )	do_( nop, "<<" )
					on_( '/' )	do_( hcn_start_LT, ">:_" )
					on_other	do_( hcn_start_LT, ">:_" )
					end
					in_( "<<" ) bgn_
						on_( '>' )	do_( nop, "<<>" )
						on_other	do_( hcn_start, "<<_" )
						end
						in_( "<<>" ) bgn_
							on_( '>' )	do_( nop, base )
							on_other	do_( hcn_start_GT, "<<_" )
							end
						in_( "<<_" ) bgn_
							on_( '>' )	do_( nop, "<<_>" )
							on_other	do_( hcn_append, same )
							end
							in_( "<<_>" ) bgn_
								on_( '>' )	do_( nop, "<<_>>" )
								on_other	do_( hcn_append_GT, "<<_" )
								end
								in_( "<<_>>" ) bgn_
									on_( ' ' )	do_( nop, same )
									on_( '\t' )	do_( nop, same )
									on_( '\n' )	do_( hcn_finish, base )
									on_other	do_( warning, "flush" )
									end
								in_( "flush" ) bgn_
									on_( '\n' )	do_( hcn_finish, base )
									on_other	do_( nop, same )
									end
				in_( ">:_" ) bgn_
					on_( '<' )	do_( nop, ">:_<" )
					on_( '\n' )	do_( hcn_finish, base )
					on_other	do_( hcn_append, same )
					end
					in_( ">:_<" ) bgn_
						on_( '<' )	do_( hcn_clear, "<<" )
						on_other	do_( hcn_append_LT, ">:_" )
						end
			end
		}
		while ( strcmp( state, "" ) );
	}
	while ( event == 0 );
	return event;
}

