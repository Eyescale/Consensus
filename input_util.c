#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "database.h"
#include "registry.h"
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
#if 1
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
	{ '_', 0 },
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
int
num_separators( void )
{
	return sizeof( separator_table ) / sizeof( separator_table[ 0 ] );
}
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

/*---------------------------------------------------------------------------
	read_token
---------------------------------------------------------------------------*/
int
read_token( char *state, int event, char **next_state, _context *context )
{
	if ( !command_mode( 0, InstructionMode, ExecutionMode ) ) {
		event = flush_input( state, event, &same, context );
		*next_state = *next_state + strlen( *next_state ) + 1;
		return event;
	}
	char *ptr = *next_state;
	for ( ; *ptr; event=0, ptr++ ) {
		event = read_input( state, event, NULL, context );
		if ( event != *ptr ) return -1;
	}
	*next_state = ++ptr;
	return 0;
}

/*---------------------------------------------------------------------------
	read_0, read_1, read_2
---------------------------------------------------------------------------*/
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

int
read_0( char *state, int event, char **next_state, _context *context )
{
	if ( !command_mode( 0, InstructionMode, ExecutionMode ) ) {
		do_( flush_input, same );
		return event;
	}

	state = base;
	do {
		event = read_input( state, event, NULL, context );
		bgn_
		in_( base ) bgn_
			on_( '\\' )	do_( nop, "\\" )
			on_( '\"' )	do_( nop, "\"" )
			on_separator	do_( error, "" )
			on_other	do_( identifier_start, "identifier" )
			end
			in_( "\\" ) bgn_
				on_any	do_( identifier_start, "identifier" )
				end
			in_( "\"" ) bgn_
				on_( '\\' )	do_( nop, "\"\\" )
				on_other	do_( identifier_start, "\"identifier" )
				end
				in_( "\"\\" ) bgn_
					on_( 't' )	event = '\t';
							do_( identifier_start, "\"identifier" )
					on_( 'n' )	event = '\n';
							do_( identifier_start, "\"identifier" )
					on_other	do_( identifier_start, "\"identifier" )
					end
				in_( "\"identifier" ) bgn_
					on_( '\\' )	do_( nop, "\"identifier\\" )
					on_( '\"' )	do_( nop, "\"identifier\"" )
					on_other	do_( identifier_append, same )
					end
					in_( "\"identifier\\" ) bgn_
						on_( 't' )	event = '\t';
								do_( identifier_append, "\"identifier" )
						on_( 'n' )	event = '\n';
								do_( identifier_append, "\"identifier" )
						on_other	do_( identifier_append, "\"identifier" )
						end
					in_( "\"identifier\"" ) bgn_
						on_any do_( identifier_finish, "" )
						end
		in_( "identifier" ) bgn_
			on_( '\\' )	do_( nop, "identifier\\" )
			on_( '\"' )	do_( identifier_finish, "" )
			on_separator	do_( identifier_finish, "" )
			on_other	do_( identifier_append, same )
			end
			in_( "identifier\\" ) bgn_
				on_( 't' )	event = '\t';
						do_( identifier_append, "identifier" )
				on_( 'n' )	event = '\n';
						do_( identifier_append, "identifier" )
				on_other	do_( identifier_append, "identifier" )
				end
		end
	}
	while ( strcmp( state, "" ) );

	return event;
}

int
read_1( char *state, int event, char **next_state, _context *context )
{
	context->identifier.current = 1;
	event = read_0( state, event, next_state, context );
	context->identifier.current = 0;
	return event;
}

int
read_2( char *state, int event, char **next_state, _context *context )
{
	context->identifier.current = 2;
	event = read_0( state, event, next_state, context );
	context->identifier.current = 0;
	return event;
}

int
test_identifier( _context *context, int i )
	{ return ( context->identifier.id[ i ].index.name ) ? 1 : 0; }
void
set_identifier( _context *context, int i, char *value )
	{ context->identifier.id[ i ].index.name = value; }
char *
get_identifier( _context *context, int i )
	{ return context->identifier.id[ i ].index.name; }
char *
take_identifier( _context *context, int i )
{
	char *retval = context->identifier.id[ i ].index.name;
	context->identifier.id[ i ].index.name = NULL;
	return retval;
}
void
free_identifier( _context *context, int i )
{
	free( context->identifier.id[ i ].index.name );
	context->identifier.id[ i ].index.name = NULL;
}

