#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include "list.h"
#include "registry.h"
#include "database.h"
#include "kernel.h"

#include "api.h"
#include "path.h"
#include "command_util.h"
#include "input.h"
#include "input_util.h"
#include "output.h"
#include "output_util.h"
#include "value.h"

// #define DEBUG

/*---------------------------------------------------------------------------
	execution engine
---------------------------------------------------------------------------*/
#define do_( a, s ) \
	event = path_execute( a, &state, event, s, context );

static int
path_execute( _action action, char **state, int event, char *next_state, _context *context )
{
	event = action( *state, event, &next_state, context );
	if ( strcmp( next_state, same ) )
		*state = next_state;
	return event;
}

/*---------------------------------------------------------------------------
	read_scheme
---------------------------------------------------------------------------*/
int
read_scheme( char *state, int event, char **next_state, _context *context )
{
	event = read_1( state, event, next_state, context );
	if ( event < 0 ) return event;

	char *identifier = take_identifier( context, 1 );
	if ( !strcmp( identifier, "file" ) ? 0 :
	     !strcmp( identifier, "session" ) ? 0 :
	     !strcmp( identifier, "operator" ) ? 0 :
	     !strcmp( identifier, "cgi" ) ? 0 : 1 )
	{
		free( identifier );
		return outputf( Error, "'%s': unknown scheme", identifier );
	}

	free( context->identifier.path );
	context->identifier.path = NULL;
	free( context->identifier.scheme );
	context->identifier.scheme = identifier;
	return event;
}

/*---------------------------------------------------------------------------
	read_path
---------------------------------------------------------------------------*/
static _action read_va_path;
static _action set_relative;
static _action build_path;

int
read_path( char *state, int event, char **next_state, _context *context )
/*
	the full path shall be built into context->identifier.path
	reading each step into context->identifier.id[ 2 ]
*/
{
	if ( command_mode( 0, 0, ExecutionMode ) ) {
		free( context->identifier.path );
		context->identifier.path = NULL;
	}
	state = base;
	do {
		event = read_input( state, event, &same, context );
#ifdef DEBUG
		outputf( Debug, "read_path: in \"%s\", on '%c'", state, event );
#endif
		bgn_
		on_( -1 )	do_( nothing, "" )
		in_( base ) bgn_
			on_( '/' )	do_( nop, "/" )
			on_( '.' )	do_( build_path, base )
			on_( '-' )	do_( build_path, base )
			on_( '$' )	do_( read_va_path, "step" )
			on_( '%' )	do_( read_va, "step" )
			on_separator	do_( nothing, "" )
			on_other	do_( read_va, "step" )
			end
			in_( "step" ) bgn_
				on_any	do_( build_path, base )
				end
		in_( "/" ) bgn_
			on_( '/' )	do_( set_relative, base )
			on_( '.' )	do_( build_path, base )
			on_( '-' )	do_( build_path, base )
			on_( '$' )	do_( read_va_path, "/step" )
			on_other	do_( read_va, "/step" )
			end
			in_( "/step" ) bgn_
				on_any	do_( build_path, base )
				end
		end
	}
	while ( strcmp( state, "" ) );

	if ( command_mode( 0, 0, ExecutionMode ) ) {
		free_identifier( context, 2 );
#ifdef DEBUG
		outputf( Debug, "read_path: returning %s", context->identifier.path );
#endif
	}
	return event;
}

static _action va2path;
static int
read_va_path( char *state, int event, char **next_state, _context *context )
{
	event = 0;
	state = base;
	do {
		event = read_input( state, event, &same, context );
#ifdef DEBUG
		outputf( Debug, "read_va_value: in \"%s\", on '%c'", state, event );
#endif
		bgn_
		on_( -1 )	do_( nothing, "" )
		on_( ' ' )	do_( nop, same )
		on_( '\t' )	do_( nop, same )
		in_( base ) bgn_
			on_( '[' )	do_( nop, "$[" )
			on_( '(' )	do_( set_results_to_nil, "$[_](" )
			on_other	do_( error, base )
			end
			in_( "$[" ) bgn_
				on_any	do_( solve_expression, "$[_" )
				end
			in_( "$[_" ) bgn_
				on_( ']' )	do_( nop, "$[_]" )
				on_other	do_( error, base )
				end
			in_( "$[_]" ) bgn_
				on_( '(' )	do_( nop, "$[_](" )
				on_other	do_( error, base )
				end
			in_( "$[_](" ) bgn_
				on_any	do_( read_va, "$[_](_" )
				end
			in_( "$[_](_" ) bgn_
				on_( ')' )	do_( va2path, "" )
				on_other	do_( error, base )
				end
		end
	}
	while ( strcmp( state, "" ) );
	return event;
}
static int
va2path( char *state, int event, char **next_state, _context *context )
{
	if ( !command_mode( 0, 0, ExecutionMode ) )
		return 0;

	char *va_name = get_identifier( context, 2 );
	if ( strcmp( va_name, "hcn" ) && strcmp( va_name, "story" ) )
		return outputf( Error, "'%s' value is not allowed in path", va_name );
	if ( context->expression.results == NULL )
		return outputf( Error, "va2path: $[(null)]( %s ) is not supported", va_name );

	Entity *e = popListItem( &context->expression.results );
	char *path = cn_va_get( e, va_name );
	if ( path == NULL ) {
		if ( e == CN.nil ) {
			asprintf( &path, "//Session.%s", va_name );
		}
		else asprintf( &path, "//Entity.%s", va_name );
	}
	else path = strdup( path );
	free( va_name );
	set_identifier( context, 2, path );
	return 0;
}

/*---------------------------------------------------------------------------
	set_relative
---------------------------------------------------------------------------*/
static int
set_relative( char *state, int event, char **next_state, _context *context )
{
	if ( !command_mode( 0, 0, ExecutionMode ) )
		return 0;

	if ( context->identifier.path == NULL ) {
		context->identifier.path = strdup( "//" );
		event = 0;
	}
	return event;
}

/*---------------------------------------------------------------------------
	build_path
---------------------------------------------------------------------------*/
static int
build_path( char *state, int event, char **next_state, _context *context )
{
	int retval = (( event == '.' ) || ( event == '-' )) ? 0 : event;
	if ( !command_mode( 0, 0, ExecutionMode ) )
		return retval;

	char *path = context->identifier.path;
	char *step = get_identifier( context, 2 );
#ifdef DEBUG
	outputf( Debug, "build_path: path=%s, step=%s", path, step );
#endif
	if ( step == NULL ) {
		if ( path == NULL ) {
			bgn_
			in_( base )	asprintf( &path, "%c", event );
			in_( "/" )	asprintf( &path, "/%c", event );
			end
		} else {
			bgn_
			in_( base )	asprintf( &path, "%s%c", path, event );
			in_( "/" )	asprintf( &path, "%s/%c", path, event );
			end
		}
	}
	else if ( path == NULL ) {
		bgn_
		in_( "step" ) bgn_
			on_( '.' )	asprintf( &path, "%s%c", step, event );
			on_( '-' )	asprintf( &path, "%s%c", step, event );
			on_other	asprintf( &path, "%s", step );
			end
		in_( "/step" ) bgn_
			on_( '.' )	asprintf( &path, "/%s%c", step, event );
			on_( '-' )	asprintf( &path, "/%s%c", step, event );
			on_other	asprintf( &path, "/%s", step );
			end
		end
	}
	else {
		bgn_
		in_( "step" ) bgn_
			on_( '.' )	asprintf( &path, "%s%s%c", path, step, event );
			on_( '-' )	asprintf( &path, "%s%s%c", path, step, event );
			on_other	asprintf( &path, "%s%s", path, step );
			end
		in_( "/step" ) bgn_
			on_( '.' )	asprintf( &path, "%s/%s%c", path, step, event );
			on_( '-' )	asprintf( &path, "%s/%s%c", path, step, event );
			on_other	asprintf( &path, "%s/%s", path, step );
			end
		end
	}
	free_identifier( context, 2 );

	free( context->identifier.path );
	context->identifier.path = path;

	return retval;
}

/*---------------------------------------------------------------------------
	cn_path
---------------------------------------------------------------------------*/
char *
cn_path( _context *context, const char *mode, char *name )
{
	if (( name[ 0 ] != '/' ) || ( name[ 1 ] != '/' )) {
		return name;
	}
	char *path;
	if ( !strcmp( mode, "w" ) ) {
		if ( context->control.session ) {
			asprintf( &path, "%s/session/%s/%s", CN_BASE_PATH, context->session.path, name+2 );
		}
		else asprintf( &path, "%s/%s", CN_BASE_PATH, name+2 );
	}
	else {
		struct stat buf;
		if ( context->control.session ) {
			asprintf( &path, "%s/session/%s/%s", CN_BASE_PATH, context->session.path, name+2 );
			if ( stat( path, &buf ) ) {
				free( path );
				asprintf( &path, "%s/default/%s", CN_BASE_PATH, name+2 );
			}
		}
		else asprintf( &path, "%s/default/%s", CN_BASE_PATH, name+2 );
	}
	return path;
}

