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

static _action hcn_output_CR;
static _action hcn_end;

static int
hcn_execute( _action action, char **state, int event, char *next_state, _context *context )
{
	event = action( *state, event, &next_state, context );

	if ( ( action == hcn_output_CR ) ||
	     ( action == hcn_end ) )
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
hcn_output_char_bgn( char *state, int event, char **next_state, _context *context )
{
	string_start( &context->hcn.buffer, '>' );
	string_append( &context->hcn.buffer, ':' );
#if 0
	if ( event != '%' ) {
		string_append( &context->hcn.buffer, '\\' );
	}
#endif
	string_append( &context->hcn.buffer, event );
	return 0;
}
static int
hcn_output_CR( char *state, int event, char **next_state, _context *context )
{
	hcn_output_char_bgn( state, '\n', next_state, context );
	string_finish( &context->hcn.buffer, 0 );
	return 0;
}
static int
hcn_command_bgn( char *state, int event, char **next_state, _context *context )
{
	string_start( &context->hcn.buffer, event );
	return 0;
}
static int
hcn_output_LT_char_bgn( char *state, int event, char **next_state, _context *context )
{
	hcn_output_char_bgn( state, '<', next_state, context );
	string_append( &context->hcn.buffer, event );
	return 0;
}
static int
hcn_command_GT_char_bgn( char *state, int event, char **next_state, _context *context )
{
	hcn_output_char_bgn( state, '>', next_state, context );
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
hcn_command_GT_char_append( char *state, int event, char **next_state, _context *context )
{
	string_append( &context->hcn.buffer, '>' );
	string_append( &context->hcn.buffer, event );
	return 0;
}
static int
hcn_output_LT_char_append( char *state, int event, char **next_state, _context *context )
{
	string_append( &context->hcn.buffer, '<' );
	string_append( &context->hcn.buffer, event );
	return 0;
}
static int
hcn_end( char *state, int event, char **next_state, _context *context )
{
	string_append( &context->hcn.buffer, '\n' );
	string_finish( &context->hcn.buffer, 1 );
	return 0;
}
static int
hcn_output_end_command_bgn( char *state, int event, char **next_state, _context *context )
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
				on_( '\n' )	do_( hcn_output_CR, same )
				on_other	do_( hcn_output_char_bgn, ">:_" )
				end
				in_( "<" ) bgn_
					on_( '<' )	do_( nop, "<<" )
					on_other	do_( hcn_output_LT_char_bgn, ">:_" )
					end
			in_( ">:_" ) bgn_
				on_( '<' )	do_( nop, ">:_<" )
				on_( '\n' )	do_( hcn_end, base )
				on_other	do_( hcn_append, same )
				end
				in_( ">:_<" ) bgn_
					on_( '<' )	do_( hcn_output_end_command_bgn, "<<" )
					on_other	do_( hcn_output_LT_char_append, ">:_" )
					end
			in_( "<<" ) bgn_
				on_( '>' )	do_( nop, "<<>" )
				on_other	do_( hcn_command_bgn, "<<_" )
				end
				in_( "<<>" ) bgn_
					on_( '>' )	do_( nop, base )
					on_other	do_( hcn_command_GT_char_bgn, "<<_" )
					end
				in_( "<<_" ) bgn_
					on_( '>' )	do_( nop, "<<_>" )
					on_other	do_( hcn_append, same )
					end
					in_( "<<_>" ) bgn_
						on_( '>' )	do_( nop, "<<_>>" )
						on_other	do_( hcn_command_GT_char_append, "<<_" )
						end
						in_( "<<_>>" ) bgn_
							on_( ' ' )	do_( nop, same )
							on_( '\t' )	do_( nop, same )
							on_( '\n' )	do_( hcn_end, base )
							on_other	do_( warning, "<<_>>_" )
							end
						in_( "<<_>>_" ) bgn_
							on_( '\n' )	do_( hcn_end, base )
							on_other	do_( nop, same )
							end
			end
		}
		while ( strcmp( state, "" ) );
	}
	while ( event == 0 );
	return event;
}

