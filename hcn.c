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
#include "output.h"
#include "hcn.h"

// #define DEBUG

/*---------------------------------------------------------------------------
	engine
---------------------------------------------------------------------------*/
#define do_( a, s ) \
	event = hcn_execute( a, &state, event, s, context );

static _action hcn_output_CR;
static _action hcn_output_end;
static _action hcn_command_end;

static int
hcn_execute( _action action, char **state, int event, char *next_state, _context *context )
{
	event = action( *state, event, &next_state, context );

	if (( action == hcn_output_CR ) || ( action == hcn_output_end ) || ( action == hcn_command_end ))
	{
		context->hcn.state = ( strcmp( next_state, same ) ? next_state : *state );
		*state = "";
	}
	else if ( strcmp( next_state, same ) ) {
		*state = next_state;
	}
	return event;
}

/*---------------------------------------------------------------------------
	output actions
---------------------------------------------------------------------------*/
static int
hcn_output_bgn( char *state, int event, char **next_state, _context *context )
{
	string_start( &context->hcn.buffer, '>' );
	string_append( &context->hcn.buffer, ':' );
	return 0;
}
static int
hcn_output_end( char *state, int event, char **next_state, _context *context )
{
	string_append( &context->hcn.buffer, '\n' );
	string_finish( &context->hcn.buffer, 0 );
	return 0;
}
static int
hcn_output_char( char *state, int event, char **next_state, _context *context )
{
	string_append( &context->hcn.buffer, event );
	return 0;
}
static int
hcn_output_special_char( char *state, int event, char **next_state, _context *context )
{
	string_append( &context->hcn.buffer, '\\' );
	string_append( &context->hcn.buffer, event );
	return 0;
}
static int
hcn_output_quote_bgn( char *state, int event, char **next_state, _context *context )
{
	string_append( &context->hcn.buffer, '\\' );
	string_append( &context->hcn.buffer, '"' );
	return 0;
}
static int
hcn_output_quote_end( char *state, int event, char **next_state, _context *context )
{
	string_append( &context->hcn.buffer, '\\' );
	string_append( &context->hcn.buffer, '"' );
	return 0;
}
static int
hcn_output_bgn_char( char *state, int event, char **next_state, _context *context )
{
	hcn_output_bgn( state, event, next_state, context );
	if (( event == ' ' ) || ( event == '\t' ))
		string_append( &context->hcn.buffer, '\\' );
	string_append( &context->hcn.buffer, event );
	return 0;
}
static int
hcn_output_bgn_special_char( char *state, int event, char **next_state, _context *context )
{
	hcn_output_bgn( state, event, next_state, context );
	string_append( &context->hcn.buffer, '\\' );
	string_append( &context->hcn.buffer, event );
	return 0;
}
static int
hcn_output_CR( char *state, int event, char **next_state, _context *context )
{
	string_start( &context->hcn.buffer, '>' );
	string_append( &context->hcn.buffer, ':' );
	string_append( &context->hcn.buffer, '\\' );
	string_append( &context->hcn.buffer, 'n' );
	string_finish( &context->hcn.buffer, 0 );
	return 0;
}
static int
hcn_output_bgn_LT( char *state, int event, char **next_state, _context *context )
{
	hcn_output_bgn( state, event, next_state, context );
	string_append( &context->hcn.buffer, '<' );
	return 0;
}
static int
hcn_output_bgn_LT_char( char *state, int event, char **next_state, _context *context )
{
	hcn_output_bgn( state, event, next_state, context );
	string_append( &context->hcn.buffer, '<' );
	string_append( &context->hcn.buffer, event );
	return 0;
}
static int
hcn_output_LT( char *state, int event, char **next_state, _context *context )
{
	string_append( &context->hcn.buffer, '<' );
	return 0;
}
static int
hcn_output_LT_char( char *state, int event, char **next_state, _context *context )
{
	string_append( &context->hcn.buffer, '<' );
	string_append( &context->hcn.buffer, event );
	return 0;
}
/*---------------------------------------------------------------------------
	output-to-command transition
---------------------------------------------------------------------------*/
static int
hcn_output_end_command_bgn( char *state, int event, char **next_state, _context *context )
{
	free( context->hcn.buffer.index.name );
	freeListItem( (listItem **) &context->hcn.buffer.value );
	return 0;
}
/*---------------------------------------------------------------------------
	command actions
---------------------------------------------------------------------------*/
static int
hcn_command_end( char *state, int event, char **next_state, _context *context )
{
	string_append( &context->hcn.buffer, '\n' );
	string_finish( &context->hcn.buffer, 0 );
	return 0;
}
static int
hcn_command_char( char *state, int event, char **next_state, _context *context )
{
	string_append( &context->hcn.buffer, event );
	return 0;
}
static int
hcn_command_bgn( char *state, int event, char **next_state, _context *context )
{
	string_start( &context->hcn.buffer, event );
	return 0;
}
static int
hcn_command_bgn_GT_char( char *state, int event, char **next_state, _context *context )
{
	string_start( &context->hcn.buffer, '>' );
	string_append( &context->hcn.buffer, event );
	return 0;
}
static int
hcn_command_GT_char( char *state, int event, char **next_state, _context *context )
{
	string_append( &context->hcn.buffer, '>' );
	string_append( &context->hcn.buffer, event );
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
	outputf( Debug, "entering hcn_getc: \"%s\"", state );
#endif

	do {
		char *state = context->hcn.state;

		// flush buffer
		// ------------
		if (( context->hcn.buffer.index.name ))
		do {
			if ( context->hcn.position == NULL ) {
				context->hcn.position = context->hcn.buffer.index.name;
#ifdef DEBUG
				outputf( Debug, "hcn: reading from: \"%s\"", (char *) context->hcn.position );
#endif
			}
			event = (int ) (( char *) context->hcn.position++ )[ 0 ];
			if ( !event ) {
				free( context->hcn.buffer.index.name );
				context->hcn.buffer.index.name = NULL;
				context->hcn.position = NULL;
			}
			else {
				return event;
			}
		}
		while (( context->hcn.buffer.index.name ));

		// input new data
		// --------------
		do {
			if ( !event ) read( fd, &event, 1 );
#ifdef DEBUG
			outputf( Debug, "hcn_getc: \"%s\", '%c'", state, event );
#endif
			bgn_
			on_( 0 ) return 0;
			in_( base ) bgn_
				on_( '<' )	do_( nop, "<" )
				on_( '\n' )	do_( hcn_output_CR, same )
				on_( '\\' )	do_( nop, "\\" )
				on_other	do_( hcn_output_bgn_char, ">:_" )
				end
				in_( "\\" ) bgn_
					on_( '\n' )	do_( nop, base )
					on_other	do_( hcn_output_bgn_special_char, ">:_" )
					end
				in_( "<" ) bgn_
					on_( '<' )	do_( nop, "<<" )
					on_( '\\' )	do_( hcn_output_bgn_LT, ">:_\\" )
					on_other	do_( hcn_output_bgn_LT_char, ">:_" )
					end
			in_( ">:_" ) bgn_
				on_( '"' )	do_( hcn_output_quote_bgn, ">:_\"" )
				on_( '\\' )	do_( nop, ">:_\\" )
				on_( '<' )	do_( nop, ">:_<" )
				on_( '\n' )	do_( hcn_output_end, base )
				on_other	do_( hcn_output_char, same )
				end
				in_( ">:_\\" ) bgn_
					on_( '\n' )	do_( nop, ">:_" )
					on_other	do_( hcn_output_special_char, ">:_" )
					end
				in_( ">:_\"" ) bgn_
					on_( '"' )	do_( hcn_output_quote_end, ">:_" )
					on_( '\\' )	do_( nop, ">:_\"\\" )
					on_other	do_( hcn_output_char, same )
					end
					in_( ">:_\"\\" ) bgn_
						on_( '\n' )	do_( nop, ">:_\"" )
						on_( '"' )	do_( hcn_output_char, ">:_\"" )
						on_other	do_( hcn_output_special_char, ">:_\"" )
						end
				in_( ">:_<" ) bgn_
					on_( '<' )	do_( hcn_output_end_command_bgn, "<<" )
					on_( '\\' )	do_( hcn_output_LT, ">:_\\" )
					on_other	do_( hcn_output_LT_char, ">:_" )
					end
			in_( "<<" ) bgn_
				on_( '>' )	do_( nop, "<<>" )
				on_other	do_( hcn_command_bgn, "<<_" )
				end
				in_( "<<>" ) bgn_
					on_( '>' )	do_( nop, base )
					on_other	do_( hcn_command_bgn_GT_char, "<<_" )
					end
				in_( "<<_" ) bgn_
					on_( '>' )	do_( nop, "<<_>" )
					on_other	do_( hcn_command_char, same )
					end
					in_( "<<_>" ) bgn_
						on_( '>' )	do_( nop, "<<_>>" )
						on_other	do_( hcn_command_GT_char, "<<_" )
						end
						in_( "<<_>>" ) bgn_
							on_( ' ' )	do_( nop, same )
							on_( '\t' )	do_( nop, same )
							on_( '\n' )	do_( hcn_command_end, base )
							on_other	do_( warning, "<<_>>_" )
							end
						in_( "<<_>>_" ) bgn_
							on_( '\n' )	do_( hcn_command_end, base )
							on_other	do_( nop, same )
							end
			end
		}
		while ( strcmp( state, "" ) );
	}
	while ( event == 0 );
	return event;
}

