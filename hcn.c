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

static _action hcn_output_bgn;
static _action hcn_output_CR;
static _action hcn_output_bgn_char;
static _action hcn_output_bgn_LT;
static _action hcn_output_bgn_LT_char;
static _action hcn_output_char;
static _action hcn_output_special_char;
static _action hcn_output_LT;
static _action hcn_output_LT_char;
static _action hcn_output_end;
static _action hcn_output_end_command_bgn;

static _action hcn_command_bgn;
static _action hcn_command_bgn_GT_char;
static _action hcn_command_char;
static _action hcn_command_GT_char;
static _action hcn_command_end;
static _action hcn_command_end_output_bgn;


/*---------------------------------------------------------------------------
	engine
---------------------------------------------------------------------------*/
#define do_( a, s ) \
	event = hcn_execute( a, &state, event, s, context );

static int
hcn_execute( _action action, char **state, int event, char *next_state, _context *context )
{
	event = action( *state, event, &next_state, context );
	if ( strcmp( next_state, same ) ) {
		if ( !strcmp( next_state, "" ) ) {
			string_finish( &context->hcn.buffer, 0 );
			context->hcn.position = context->hcn.buffer.value;
			context->hcn.state = base;
		}
		*state = next_state;
	}
	return event;
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
		if (( context->hcn.position )) {
			event = (int ) (( char *) context->hcn.position++ )[ 0 ];
			if ( !event ) {
				string_start( &context->hcn.buffer, 0 );
				context->hcn.position = NULL;
			}
			else return event;
		}

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
				on_( '\\' )	do_( hcn_output_bgn, ">:_\\" )
				on_( '\n' )	do_( hcn_output_CR, same )
				on_other	do_( hcn_output_bgn_char, ">:_" )
				end
				in_( "<" ) bgn_
					on_( '<' )	do_( nop, "<<" )
					on_( '\\' )	do_( hcn_output_bgn_LT, ">:_\\" )
					on_other	do_( hcn_output_bgn_LT_char, ">:_" )
					end
			in_( ">:_" ) bgn_
				on_( '\\' )	do_( nop, ">:_\\" )
				on_( '<' )	do_( nop, ">:_<" )
				on_( '\n' )	do_( hcn_output_end, base )
				on_other	do_( hcn_output_char, same )
				end
				in_( ">:_\\" ) bgn_
					on_any	do_( hcn_output_special_char, ">:_" )
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
							on_other	do_( hcn_command_end_output_bgn, same )
							end
			end
		}
		while ( strcmp( state, "" ) );
	}
	while ( !event );
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
hcn_output_CR( char *state, int event, char **next_state, _context *context )
{
	hcn_output_bgn( state, event, next_state, context );
	string_append( &context->hcn.buffer, '\\' );
	string_append( &context->hcn.buffer, 'n' );
	return hcn_output_end( state, event, next_state, context );
}
static int
hcn_output_bgn_char( char *state, int event, char **next_state, _context *context )
{
	hcn_output_bgn( state, event, next_state, context );
	switch ( event ) {
	case ' ':
	case '\t':
		string_append( &context->hcn.buffer, '\\' );
		break;
	}
	return hcn_output_char( state, event, next_state, context );
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
	hcn_output_bgn_LT( state, event, next_state, context );
	return hcn_output_char( state, event, next_state, context );
}
static int
hcn_output_char( char *state, int event, char **next_state, _context *context )
{
	switch ( event ) {
	case '\n':
		return hcn_output_end( state, event, next_state, context );
	case '\"':
		string_append( &context->hcn.buffer, '%' );
		string_append( &context->hcn.buffer, '\'' );
		break;
	case '#':
	case '|':
	case '\\':
		string_append( &context->hcn.buffer, '\\' );
		string_append( &context->hcn.buffer, event );
		break;
	default:
		string_append( &context->hcn.buffer, event );
		break;
	}
	return 0;
}
static int
hcn_output_special_char( char *state, int event, char **next_state, _context *context )
{
	switch ( event ) {
	case '%':
	case '$':
		string_append( &context->hcn.buffer, '\\' );
		string_append( &context->hcn.buffer, event );
		return 0;
	default:
		string_append( &context->hcn.buffer, '\\' );
		string_append( &context->hcn.buffer, '\\' );
		return hcn_output_char( state, event, next_state, context );
	}
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
	hcn_output_LT( state, event, next_state, context );
	return hcn_output_char( state, event, next_state, context );
}
static int
hcn_output_end( char *state, int event, char **next_state, _context *context )
{
	string_append( &context->hcn.buffer, '\n' );
	*next_state = "";
	return 0;
}
static int
hcn_output_end_command_bgn( char *state, int event, char **next_state, _context *context )
{
	// DO_LATER: flush output
	if (( context->hcn.buffer.index.value == STRING_EVENTS ) && ( context->hcn.buffer.value )) {
		int flag = 3;
		for ( listItem *i = context->hcn.buffer.value; i!=NULL; i=i->next ) {
			switch ((int) i->ptr ) {
			case ' ':
			case '\t':
				if ( flag != 3 )
					{ flag = 1; i = NULL; }
				break;
			case '\\':
				if ( flag == 3 ) flag--;
				else { flag = 1; i = NULL; }
				break;
			case ':':
				if ( flag == 2 ) flag--;
				else { flag = 1; i = NULL; }
				break;
			case '>':
				if ( flag == 1 ) flag--;
				else { flag = 1; i = NULL; }
				break;
			default:
				{ flag = 1; i = NULL; }
			}
		}
		if ( flag ) {
			output( Warning, "hcn_getc: << must start on a blank line - ignoring leading characters" );
		}
	}
	return string_start( &context->hcn.buffer, 0 );
}
/*---------------------------------------------------------------------------
	command actions
---------------------------------------------------------------------------*/
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
hcn_command_char( char *state, int event, char **next_state, _context *context )
{
	string_append( &context->hcn.buffer, event );
	return 0;
}
static int
hcn_command_GT_char( char *state, int event, char **next_state, _context *context )
{
	string_append( &context->hcn.buffer, '>' );
	return hcn_command_char( state, event, next_state, context );
}
static int
hcn_command_end( char *state, int event, char **next_state, _context *context )
{
	string_append( &context->hcn.buffer, '\n' );
	*next_state = "";
	return 0;
}
static int
hcn_command_end_output_bgn( char *state, int event, char **next_state, _context *context )
{
	// DO_LATER: flush command and begin new output
	switch ( event ) {
		case ' ':
		case '\t':
			break;
		default:
			output( Warning, "hcn_getc: ignoring >> trailing characters" );
	}
	return 0;
}
