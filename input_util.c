#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

#include "list.h"
#include "registry.h"
#include "database.h"
#include "kernel.h"

#include "input.h"
#include "input_util.h"
#include "output.h"

// #define DEBUG

struct {
	int real, fake;
}
separator_table[] = {
	{ '\t', 't' },
	{ '\n', 'n' },
#if 0
	{ '\\', '\\' },
#endif
	{ '\0', 0 },
	{ -1, 0 },
	{ ' ', 0 },
	{ '.', 0 },
	{ ':', 0 },
	{ ',', 0 },
	{ ';', 0 },
	{ '/', 0 },
	{ '?', 0 },
	{ '!', 0 },
	{ '%', 0 },
	{ '~', 0 },
	{ '*', 0 },
#if 0
	{ '_', 0 },	// allowed inside identifiers
#endif
	{ '-', 0 },
	{ '+', 0 },
	{ '=', 0 },
	{ '|', 0 },
	{ '<', 0 },
	{ '>', 0 },
	{ '[', 0 },
	{ ']', 0 },
	{ '(', 0 },
	{ ')', 0 },
	{ '{', 0 },
	{ '}', 0 },
	{ '[', 0 },
	{ ']', 0 },
	{ '&', 0 },
};

/*---------------------------------------------------------------------------
	execution engine	- local
---------------------------------------------------------------------------*/
#define do_( a, s ) \
	event = read_execute( a, &state, event, s, context );

static int
read_execute( _action action, char **state, int event, char *next_state, _context *context )
{
	event = action( *state, event, &next_state, context );
	if ( strcmp( next_state, same ) )
		*state = next_state;
	return event;
}

/*---------------------------------------------------------------------------
	is_separator
---------------------------------------------------------------------------*/
static int num_separators( void );

int
is_separator( int event )
{
	for ( int i =0; i< num_separators(); i++ ) {
		if ( event == separator_table[ i ].real ) {
			return event;
		}
	}
	return !event;
}

static int
num_separators( void )
{
	return sizeof( separator_table ) / sizeof( separator_table[ 0 ] );
}

/*---------------------------------------------------------------------------
	read_token
---------------------------------------------------------------------------*/
int
read_token( char *state, int event, char **next_state, _context *context )
{
	char *ptr = *next_state;
	for ( ; *ptr; event=0, ptr++ ) {
		event = read_input( state, event, &same, context );
		if ( event != *ptr ) return -1;
	}
	*next_state = ++ptr;
	return 0;
}

/*---------------------------------------------------------------------------
	read_0, read_1, read_2
---------------------------------------------------------------------------*/
static _action read_identifier;
int
read_0( char *state, int event, char **next_state, _context *context )
{
	context->identifier.current = 0;
	event = read_identifier( state, event, next_state, context );
	return event;
}
int
read_1( char *state, int event, char **next_state, _context *context )
{
	context->identifier.current = 1;
	event = read_identifier( state, event, next_state, context );
	context->identifier.current = 0;
	return event;
}
int
read_2( char *state, int event, char **next_state, _context *context )
{
	context->identifier.current = 2;
	event = read_identifier( state, event, next_state, context );
	context->identifier.current = 0;
	return event;
}

/*---------------------------------------------------------------------------
	read_identifier		- local
---------------------------------------------------------------------------*/
static _action identifier_start;
static _action identifier_append;
static _action identifier_finish;

static int
read_identifier( char *state, int event, char **next_state, _context *context )
/*
	Note that we must read even in FreezeMode, as the program logics in some
	case depends on the identifier (e.g. in case of value accounts), whereas
	we must count the push(es) and pop(s) to know when to exit FreezeMode.
*/
{
	state = base;
	identifier_start( state, 0, &same, context );
	do {
		event = read_input( state, event, &same, context );
#ifdef DEBUG
		outputf( Debug, "read_identifier: in \"%s\", on '%c'", state, event );
#endif
		bgn_
		on_( 0 ) do_( nothing, "" )
		in_( base ) bgn_
			on_( '\\' )	do_( nothing, "identifier" )
			on_( '\"' )	do_( nop, "\"" )
			on_separator	do_( error, "" )
			on_other	do_( identifier_append, "identifier" )
			end
			in_( "\"" ) bgn_
				on_( '\\' )	do_( nop, "\"_\\" )
				on_( '\"' )	do_( nop, "\"_\"" )
				on_other	do_( identifier_append, "\"identifier" )
				end
				in_( "\"_\\" ) bgn_
#ifdef CR_ENCODE
					on_( 'n' )	event = '\n';
							do_( identifier_append, "\"identifier" )
					on_other	do_( identifier_append, "\"identifier" )
#else
					on_any	do_( identifier_append, "\"identifier" )
#endif
					end
				in_( "\"_\"" ) bgn_
					on_any do_( identifier_finish, "" )
					end
				in_( "\"identifier" ) bgn_
					on_( '\\' )	do_( nop, "\"_\\" )
					on_( '\"' )	do_( nop, "\"_\"" )
					on_other	do_( identifier_append, same )
					end
		in_( "identifier" ) bgn_
			on_( '\\' )	do_( nop, "_\\" )
			on_( '\"' )	do_( identifier_finish, "" )
			on_separator	do_( identifier_finish, "" )
			on_other	do_( identifier_append, same )
			end
			in_( "_\\" ) bgn_
				on_( '0' )	do_( nop, "_\\0" )
				on_( 't' )	event = '\t';
						do_( identifier_append, "identifier" )
				on_( 'n' )	event = '\n';
						do_( identifier_append, "identifier" )
				on_other	do_( identifier_append, "identifier" )
				end
			in_( "_\\0" ) bgn_
				on_any	do_( identifier_finish, "" )
				end
		end
	}
	while ( strcmp( state, "" ) );

	return event;
}

static int
identifier_start( char *state, int event, char **next_state, _context *context )
{
	return string_start( &context->identifier.id[ context->identifier.current ], event );
}
static int
identifier_append( char *state, int event, char **next_state, _context *context )
{
	return string_append( &context->identifier.id[ context->identifier.current ], event );
}
static int
identifier_finish( char *state, int event, char **next_state, _context *context )
{
	string_finish( &context->identifier.id[ context->identifier.current ], 0 );
	return event;
}

/*---------------------------------------------------------------------------
	test_, set_, get_ take_ and free_identifier
---------------------------------------------------------------------------*/
int
test_identifier( _context *context, int i )
{
	if (( context->identifier.id[ i ].index.value == STRING_TEXT ) &&
	    ( context->identifier.id[ i ].value != NULL )) {
		return 1;
	}
	return 0;
}
void
set_identifier( _context *context, int i, char *value )
{
	context->identifier.id[ i ].index.value = STRING_TEXT;
	context->identifier.id[ i ].value = value;
}
char *
get_identifier( _context *context, int i )
{
	if ( context->identifier.id[ i ].index.value == STRING_TEXT ) {
		return context->identifier.id[ i ].value;
	}
	return NULL;
}
char *
take_identifier( _context *context, int i )
{
	if ( context->identifier.id[ i ].index.value == STRING_TEXT ) {
		char *retval = context->identifier.id[ i ].value;
		context->identifier.id[ i ].value = NULL;
		return retval;
	}
	return NULL;
}
void
free_identifier( _context *context, int i )
{
	if ( context->identifier.id[ i ].index.value == STRING_TEXT ) {
		free( context->identifier.id[ i ].value );
		context->identifier.id[ i ].value = NULL;
	}
}

