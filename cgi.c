#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <unistd.h>

#include "database.h"
#include "registry.h"
#include "kernel.h"

#include "cgi.h"
#include "expression.h"
#include "expression_util.h"
#include "frame.h"
#include "input.h"
#include "input_util.h"
#include "narrative_util.h"
#include "output.h"
#include "api.h"

// #define DEBUG

/*---------------------------------------------------------------------------
	execution engine	- local
---------------------------------------------------------------------------*/
#define do_( a, s ) \
	event = execute( a, &state, event, s, context );

static int
execute( _action action, char **state, int event, char *next_state, _context *context )
{
	int retval;
	if ( action == push ) {
		event = push( *state, event, &next_state, context );
		*state = base;
	} else {
		event = action( *state, event, &next_state, context );
		if ( strcmp( next_state, same ) )
			*state = next_state;
	}
	return event;
}

/*---------------------------------------------------------------------------
	read_cgi_output_session
---------------------------------------------------------------------------*/
int
read_cgi_output_session( char *state, int event, char **next_state, _context *context )
{
	if ( strcmp( state, ">@ %session" ) || ( event != ':' ))
		return 0;

	event = 0;
	state = ">@ %session:";
	do {
		event = read_input( state, event, next_state, context );
		bgn_
		on_( ' ' )	do_( nop, same )
		on_( '\t' )	do_( nop, same )
		in_( ">@ %session:" ) bgn_
			on_( '.' )	do_( nop, ">@ %session: ." )
			on_other	do_( error, "" )
			end
		in_( ">@ %session: ." ) bgn_
			on_( '|' )	do_( nop, ">@ %session: . |" )
			on_other	do_( error, "" )
			end
		in_( ">@ %session: . |" ) bgn_
			on_( '>' )	do_( nop, ">@ %session: . | >" )
			on_other	do_( error, "" )
			end
		in_( ">@ %session: . | >" ) bgn_
			on_( ':' )	do_( nop, ">@ %session: . | >:" )
			on_other	do_( error, "" )
			end
		in_( ">@ %session: . | >:" ) bgn_
			on_( '\n' )	do_( nothing, "" )
			on_other	do_( error, "" )
			end
		end
	}
	while ( strcmp( state, "" ) );

	return event;
}

/*---------------------------------------------------------------------------
	cgi_output_session
---------------------------------------------------------------------------*/
int
cgi_output_session( char *session )
{
	int event = cn_dof( ">@ %s:\\<cgi> %%[ . ] | >:", session );
	return ( event < 0 ) ? event : '\n';
}

/*---------------------------------------------------------------------------
	cgi_scheme_op_pipe
---------------------------------------------------------------------------*/
int
cgi_scheme_op_pipe( char *state, int event, char **next_state, _context *context )
{
	char *scheme = context->expression.scheme;
	if ( strcmp( scheme, "session" ) )
		return outputf( Error, "%s operation not supported", scheme );
	if ( context->expression.mode != InstantiateMode )
		return output( Error, "session operation not supported" );
#ifdef DO_LATER
	check session's path format (must not be a number)
#endif
	if ( !command_mode( 0, 0, ExecutionMode ) )
		return 0;
	if ( context->control.operator_absent )
		return output( Error, "operator out of order" );

	char *session = context->identifier.path;
	int retval = cn_dof( ">@ operator: !! session:%s |", session );
#ifdef DO_LATER
	check return value - e.g. error or session exists already
#endif
	retval = cn_dof( ">@%s:< \\%.$( html ) | >:", session );
#ifdef DO_LATER
	check return value - e.g. error or session exists already
#endif
	return 0;
}

/*---------------------------------------------------------------------------
	read_cgi_input
---------------------------------------------------------------------------*/
static int
add_cgi_entry( char *state, int event, char **next_state, _context *context )
{
	Expression *expression = makeLiteral( context->expression.ptr );
	if (( expression )) {
		FrameLog *log = &context->frame.log.cgi;
		addItem( &log->entities.instantiated, &expression->sub[ 3 ] );
		context->expression.ptr = NULL;
	}
	return 0;
}

static void
free_cgi_log( _context *context )
{
	FrameLog *log = &context->frame.log.cgi;
	for ( listItem *i=log->entities.instantiated; i!=NULL; i=i->next )
		freeLiteral( i->ptr );
	freeListItem( &log->entities.instantiated );
}

int
read_cgi_input( char *state, int event, char **next_state, _context *context )
{
	if ( !context->control.session )
		return output( Error, "< cgi >: not in a session" );
	if ( !command_mode( 0, 0, ExecutionMode ) )
		return output( Error, "< cgi >: not in execution mode" );

	// build the log of cgi entries
	// ----------------------------
	event = 0;
	state = "< cgi >:";
	do {
		event = read_input( state, event, &same, context );
		bgn_
		on_( -1 )	do_( nothing, "" )
		on_( ' ' )	do_( nop, same )
		on_( '\t' )	do_( nop, same )
		in_( "< cgi >:" ) bgn_
			on_( '{' )	do_( nop, "< cgi >: {" )
			on_other	do_( read_expression, "< cgi >:_" )
			end
		in_( "< cgi >:_" ) bgn_
			on_( '\n' )	do_( add_cgi_entry, "" )
			on_other	do_( error, "" )
			end
		in_( "< cgi >: {" ) bgn_
			on_any		do_( read_expression, "< cgi >: {_" )
			end
			in_( "< cgi >: {_" ) bgn_
				on_( ',' )	do_( add_cgi_entry, "< cgi >: {" )
				on_( '}' )	do_( add_cgi_entry, "< cgi >: {_}" )
				on_other	do_( error, "" )
				end
			in_( "< cgi >: {_}" ) bgn_
				on_( '\n' )	do_( nop, "" )
				on_other	do_( error, "" )
				end
		end
	}
	while ( strcmp( state, "" ) );
	if ( event < 0 )
		free_cgi_log( context );

	// process cgi entries
	// -------------------
	FrameLog *log = &context->frame.log.cgi;
	if ( log->entities.instantiated == NULL )
		return event;

	Narrative *n = lookupNarrative( CN.nil, "cgi" );
	if ( n == NULL ) {
		event = outputf( Error, "session: %s - cgi story not found", context->session.path );
	}
	else {
		activateNarrative( CN.nil, n );
		do { narrative_process( n, log, context ); }
		while ( !n->deactivate );
		deactivateNarrative( CN.nil, n );
	}

	free_cgi_log( context );
	return event;
}

