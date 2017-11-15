#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "database.h"
#include "registry.h"
#include "kernel.h"

#include "path.h"
#include "expression.h"
#include "expression_util.h"
#include "input.h"
#include "input_util.h"
#include "output.h"
#include "output_util.h"
#include "variable.h"
#include "variable_util.h"

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
	read_STEP	- local utility
---------------------------------------------------------------------------*/
static void
read_STEP( _context * context )
{
	if ( context->output.slist == NULL )
		return;

	listItem **list = &context->output.slist;
	context->identifier.id[ 2 ].ptr = popListItem( list );
	for ( listItem *i = *list; i!=NULL; i=i->next )
		free( i->ptr );
	freeListItem( list );
}

/*---------------------------------------------------------------------------
	va2s	- translate variable value into string
---------------------------------------------------------------------------*/
static int
va2s( char *state, int event, char **next_state, _context *context )
{
	if ( !command_mode( 0, 0, ExecutionMode ) )
		return event;

	char *identifier = context->identifier.id[ 1 ].ptr;
	context->identifier.id[ 1 ].ptr = NULL;

	VariableVA *variable = lookupVariable( context, identifier, 0 );
	if ( variable == NULL ) {
		free( identifier );
		return outputf( Error, "read_path: variable '%s' not found - cannot form path", identifier );
	}
	listItem *list = NULL;

	switch ( variable->type ) {
	case StringVariable:
	case NarrativeVariable:
		event = outputf( Error, "read_path: unsupported variable type - '%s'", identifier );
		free( identifier );
		return event;
	case EntityVariable:
		// take only the first item in the list of entities
		list = variable->value;
		if ( list != NULL ) {
			push_output( "", NULL, StringOutput, context );
			output_entity_name( list->ptr, NULL, 1 );
			pop_output( context, 0 );
			read_STEP( context );
		}
		break;
	case ExpressionVariable:
		// take only the first item in the list of expressions
		list = variable->value;
		if ( list != NULL ) {
			context->expression.mode = EvaluateMode;
	                expression_solve( list->ptr, 3, context );
		        list = context->expression.results;
		}
		// take only the first item in the list of results
		if ( list != NULL ) {
			push_output( "", NULL, StringOutput, context );
			output_entity_name( list->ptr, NULL, 1 );
			pop_output( context, 0 );
			read_STEP( context );
		}
		break;
	case LiteralVariable:
		// take only the first item in the list of literals
		list = variable->value;
		if ( list != NULL ) {
			ExpressionSub *s = (ExpressionSub *) list->ptr;
			push_output( "", NULL, StringOutput, context );
			output_expression( ExpressionAll, s->e );
			pop_output( context, 0 );
			read_STEP( context );
		}
		break;
	}
	free( identifier );
	return event;
}

/*---------------------------------------------------------------------------
	xr2s	- translate expression results into string
---------------------------------------------------------------------------*/
static int
xr2s( char *state, int event, char **next_state, _context *context )
{
	if ( !command_mode( 0, 0, ExecutionMode ) )
		return event;

	free( context->identifier.id[ 2 ].ptr );
	context->identifier.id[ 2 ].ptr = NULL;

	// take only the first item in the list of results
	if ( context->expression.results != NULL ) {
		push_output( "", NULL, StringOutput, context );
		output_entity_name( context->expression.results->ptr, NULL, 1 );
		pop_output( context, 0 );
		read_STEP( context );
	}
	return event;
}

/*---------------------------------------------------------------------------
	xeval	- read & evaluate expression from stdin
---------------------------------------------------------------------------*/
static int
xeval( char *state, int event, char **next_state, _context *context )
{
	char *backup = context->identifier.id[ 2 ].ptr;
	context->identifier.id[ 2 ].ptr = NULL;

	event = read_expression( state, event, &same, context );
	if ( event < 0 )
		;
	else if ( command_mode( 0, 0, ExecutionMode ) )
	{
		context->expression.mode = EvaluateMode;
		int retval = expression_solve( context->expression.ptr, 3, context );
		if ( retval < 0 ) event = retval;
	}

	free( context->identifier.id[ 2 ].ptr );
	context->identifier.id[ 2 ].ptr = backup;
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
	char *step = context->identifier.id[ 2 ].ptr;
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
	} else if ( path == NULL ) {
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
	} else {
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
	free( context->identifier.id[ 2 ].ptr );
	context->identifier.id[ 2 ].ptr = NULL;

	free( context->identifier.path );
	context->identifier.path = path;

	return retval;
}

/*---------------------------------------------------------------------------
	set_relative
---------------------------------------------------------------------------*/
int
set_relative( char *state, int event, char **next_state, _context *context )
{
	if ( context->identifier.path == NULL ) {
		context->identifier.path = strdup( "//" );
		event = 0;
	}
	return event;
}

/*---------------------------------------------------------------------------
	read_path
---------------------------------------------------------------------------*/
int
read_path( char *state, int event, char **next_state, _context *context )
{
	if ( command_mode( FreezeMode, 0, 0 ) ) {
		event = flush_input( state, event, &same, context );
		return event;
	}

	// the full path shall be built into context->identifier.path
	// reading each step into context->identifier.id[ 2 ]

	free( context->identifier.path );
	context->identifier.path = NULL;

	state = base;
	do {
		event = read_input( state, event, NULL, context );
#ifdef DEBUG
		outputf( Debug, "read_path: in \"%s\", on '%c'", state, event );
#endif
		bgn_
		on_( -1 )	do_( nothing, "" )
		in_( base ) bgn_
			on_( '/' )	do_( nop, "/" )
			on_( '%' )	do_( nop, "%" )
			on_( '-' )	do_( build_path, base )
			on_( '.' )	do_( build_path, base )
			on_separator	do_( nothing, "" )
			on_other	do_( read_2, "step" )
			end
			in_( "step" ) bgn_
				on_any	do_( build_path, base )
				end
			in_( "/" ) bgn_
				on_( '/' )	do_( set_relative, base )
				on_( '%' )	do_( nop, "/%" )
				on_( '.' )	do_( build_path, base )
				on_( '-' )	do_( build_path, base )
				on_other	do_( read_2, "/step" )
				end
				in_( "/step" ) bgn_
					on_any	do_( build_path, base )
					end
				in_( "/%" ) bgn_
					on_( '[' )	do_( xeval, "/%[_" )
					on_other	do_( read_variable_ref, "/%variable" )
					end
					in_( "/%[_" ) bgn_
						on_( ' ' )	do_( nop, same )
						on_( '\t' )	do_( nop, same )
						on_( ']' )	do_( nop, "/%[_]" )
						on_other	do_( error, "" )
						end
					in_( "/%variable" ) bgn_
						on_any	do_( va2s, "/step" )
						end
					in_( "/%[_]" ) bgn_
						on_any	do_( xr2s, "/step" )
						end
			in_( "%" ) bgn_
				on_( '[' )	do_( xeval, "%[_" )
				on_other	do_( read_variable_ref, "%variable" )
				end
				in_( "/%[_" ) bgn_
					on_( ' ' )	do_( nop, same )
					on_( '\t' )	do_( nop, same )
					on_( ']' )	do_( nop, "/%[_]" )
					on_other	do_( error, "" )
					end
				in_( "%variable" ) bgn_
					on_any	do_( va2s, "step" )
					end
				in_( "%[_]" ) bgn_
					on_any	do_( xr2s, "step" )
					end
		end
	}
	while ( strcmp( state, "" ) );

	free( context->identifier.id[ 2 ].ptr );
	context->identifier.id[ 2 ].ptr = NULL;

#ifdef DEBUG
	outputf( Debug, "read_path: returning %s", context->identifier.path.ptr );
#endif
	return event;
}

