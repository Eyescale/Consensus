#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "database.h"
#include "registry.h"
#include "kernel.h"

#include "path.h"
#include "expression.h"
#include "input.h"
#include "input_util.h"
#include "output.h"
#include "variables.h"

// #define DEBUG
#define PATH	1
#define STEP	2

/*---------------------------------------------------------------------------
	execution engine
---------------------------------------------------------------------------*/
#define path_do_( a, s ) \
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
	va2s	- translate variable value into string
---------------------------------------------------------------------------*/
static int
va2s( char *state, int event, char **next_state, _context *context )
{
	if ( !context_check( 0, 0, ExecutionMode ) )
		return event;

	char *identifier = context->identifier.id[ STEP ].ptr;
	registryEntry *entry = lookupVariable( context, identifier );
	if ( entry == NULL ) {
		return outputf( Error, "read_path: variable '%%%s' is empty - cannot form path", identifier );
	}
	context->identifier.id[ STEP ].ptr = NULL;

	VariableVA *variable = entry->value;
	listItem *list = NULL;
	switch ( variable->type ) {
	case NarrativeVariable:
		free( identifier );
		return outputf( Error, "read_path: variable '%%%s' holds narrative(s) - cannot form path", identifier );
	case EntityVariable:
		// take only the first item in the list of entities
		list = variable->data.value;
		if ( list != NULL ) {
			push_output( "", &context->identifier.id[ STEP ], IdentifierOutput, context );
			output_entity_name( list->ptr, NULL, 1 );
			pop_output( context );
		}
		break;
	case ExpressionVariable:
		// take only the first item in the list of expressions
		list = variable->data.value;
		if ( list != NULL ) {
			context->expression.mode = EvaluateMode;
	                expression_solve( list->ptr, 3, context );
		        list = context->expression.results;
		}
		// take only the first item in the list of results
		if ( list != NULL ) {
			push_output( "", &context->identifier.id[ STEP ], IdentifierOutput, context );
			output_entity_name( list->ptr, NULL, 1 );
			pop_output( context );
		}
		break;
	case LiteralVariable:
		// take only the first item in the list of literals
		list = variable->data.value;
		if ( list != NULL ) {
			ExpressionSub *s = (ExpressionSub *) list->ptr;
			push_output( "", &context->identifier.id[ STEP ], IdentifierOutput, context );
			output_expression( ExpressionAll, s->e, -1, -1 );
			pop_output( context );
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
	if ( !context_check( 0, 0, ExecutionMode ) )
		return event;

	free( context->identifier.id[ STEP ].ptr );
	context->identifier.id[ STEP ].ptr = NULL;

	// take only the first item in the list of results
	if ( context->expression.results != NULL ) {
		push_output( "", &context->identifier.id[ STEP ], IdentifierOutput, context );
		output_entity_name( context->expression.results->ptr, NULL, 1 );
		pop_output( context );
	}
	return event;
}

/*---------------------------------------------------------------------------
	xeval	- read & evaluate expression from stdin
---------------------------------------------------------------------------*/
static int
xeval( char *state, int event, char **next_state, _context *context )
{
	// read_expression uses read_1 for parsing
	char *backup = context->identifier.id[ STEP ].ptr;
	path_do_( read_expression, same );
	if ( !context_check( 0, 0, ExecutionMode ) || ( context->expression.mode == ErrorMode ))
		goto RETURN;

	context->expression.mode = EvaluateMode;
	int retval = expression_solve( context->expression.ptr, 3, context );
	if ( retval < 0 ) event = retval;
RETURN:
	free( context->identifier.id[ STEP ].ptr );
	context->identifier.id[ STEP ].ptr = backup;
	return event;
}

/*---------------------------------------------------------------------------
	build_path
---------------------------------------------------------------------------*/
static int
build_path( char *state, int event, char **next_state, _context *context )
{
	if ( !context_check( 0, 0, ExecutionMode ) )
		return event;

	char *path = context->identifier.id[ PATH ].ptr;
	char *step = context->identifier.id[ STEP ].ptr;
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
	free( context->identifier.id[ PATH ].ptr );
	free( context->identifier.id[ STEP ].ptr );
	context->identifier.id[ PATH ].ptr = path;
	context->identifier.id[ STEP ].ptr = NULL;

	if (( event == '.' ) || ( event == '-' ))
		event = 0;

	return event;
}

/*---------------------------------------------------------------------------
	read_path
---------------------------------------------------------------------------*/
int
read_path( char *state, int event, char **next_state, _context *context )
{
	if ( context_check( FreezeMode, 0, 0 ) ) {
		path_do_( flush_input, same );
		return event;
	}

	// the full path shall be built into context->identifier.id[ PATH ]
	// reading each step into context->identifier.id[ STEP ]

	free( context->identifier.id[ PATH ].ptr );
	context->identifier.id[ PATH ].ptr = NULL;

	state = base;
	do {
		event = input( state, event, NULL, context );
#ifdef DEBUG
		outputf( Debug, "read_path: in \"%s\", on '%c'", state, event );
#endif
		bgn_
		on_( -1 )	path_do_( nothing, "" )
		in_( base ) bgn_
			on_( '/' )	path_do_( nop, "/" )
			on_( '%' )	path_do_( nop, "%" )
			on_( '-' )	path_do_( build_path, base )
			on_( '.' )	path_do_( build_path, base )
			on_separator	path_do_( nothing, "" )
			on_other	path_do_( read_2, "step" )
			end
			in_( "step" ) bgn_
				on_any	path_do_( build_path, base )
				end
			in_( "/" ) bgn_
				on_( '/' )	path_do_( nop, base )
				on_( '%' )	path_do_( nop, "/%" )
				on_( '.' )	path_do_( build_path, base )
				on_( '-' )	path_do_( build_path, base )
				on_other	path_do_( read_2, "/step" )
				end
				in_( "/step" ) bgn_
					on_any	path_do_( build_path, base )
					end
				in_( "/%" ) bgn_
					on_( '[' )	path_do_( xeval, "/%[_" )
					on_other	path_do_( read_2, "/%variable" )
					end
					in_( "/%[_" ) bgn_
						on_( ' ' )	path_do_( nop, same )
						on_( '\t' )	path_do_( nop, same )
						on_( ']' )	path_do_( nop, "/%[_]" )
						on_other	path_do_( error, "" )
						end
					in_( "/%variable" ) bgn_
						on_any	path_do_( va2s, "/step" )
						end
					in_( "/%[_]" ) bgn_
						on_any	path_do_( xr2s, "/step" )
						end
			in_( "%" ) bgn_
				on_( '[' )	path_do_( xeval, "%[_" )
				on_other	path_do_( read_2, "%variable" )
				end
				in_( "/%[_" ) bgn_
					on_( ' ' )	path_do_( nop, same )
					on_( '\t' )	path_do_( nop, same )
					on_( ']' )	path_do_( nop, "/%[_]" )
					on_other	path_do_( error, "" )
					end
				in_( "%variable" ) bgn_
					on_any	path_do_( va2s, "step" )
					end
				in_( "%[_]" ) bgn_
					on_any	path_do_( xr2s, "step" )
					end
		end
	}
	while ( strcmp( state, "" ) );

	free( context->identifier.id[ STEP ].ptr );
	context->identifier.id[ STEP ].ptr = NULL;

#ifdef DEBUG
	outputf( Debug, "read_path: returning %s", context->identifier.id[ PATH ].ptr );
#endif
	return event;
}

