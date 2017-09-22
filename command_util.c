#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "database.h"
#include "registry.h"
#include "kernel.h"
#include "string_util.h"

#include "input.h"
#include "output.h"
#include "api.h"
#include "expression.h"
#include "narrative.h"
#include "variables.h"
#include "value.h"

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
	command expression actions
---------------------------------------------------------------------------*/
int
set_results_to_nil( char *state, int event, char **next_state, _context *context )
{
	if ( !context_check( 0, 0, ExecutionMode ) )
		return 0;

	freeListItem( &context->expression.results );
	context->expression.results = newItem( CN.nil );
	return 0;
}

int
set_expression_mode( char *state, int event, char **next_state, _context *context )
{
	if ( !context_check( 0, InstructionMode, ExecutionMode ) )
		return 0;

	switch ( event ) {
	case '!': context->expression.mode = InstantiateMode; break;
	case '~': context->expression.mode = ReleaseMode; break;
	case '*': context->expression.mode = ActivateMode; break;
	case '_': context->expression.mode = DeactivateMode; break;
	}
	return 0;
}

int
expression_op( char *state, int event, char **next_state, _context *context )
{
	if ( !context_check( 0, 0, ExecutionMode ) )
		return 0;

	int retval = expression_solve( context->expression.ptr, 3, context );
	if ( retval == -1 ) return output( Error, NULL );

	switch ( context->expression.mode ) {
	case InstantiateMode:
		// already done during expression_solve
		break;
	case ReleaseMode:
		for ( listItem *i = context->expression.results; i!=NULL; i=i->next ) {
			Entity *e = (Entity *) i->ptr;
			cn_release( e );
		}
		freeListItem( &context->expression.results );
		break;
	case ActivateMode:
		for ( listItem *i = context->expression.results; i!=NULL; i=i->next ) {
			Entity *e = (Entity *) i->ptr;
			cn_activate( e );
		}
		break;
	case DeactivateMode:
		for ( listItem *i = context->expression.results; i!=NULL; i=i->next ) {
			Entity *e = (Entity *) i->ptr;
			cn_deactivate( e );
		}
		break;
	default:
		break;
	}
	return event;
}

int
evaluate_expression( char *state, int event, char **next_state, _context *context )
{
	switch ( context->control.mode ) {
	case FreezeMode:
		do_( flush_input, same );
		return event;
	case InstructionMode:
		do_( parse_expression, same );
		return event;
	case ExecutionMode:
		break;
	}

	int restore_mode = 0;
	context->expression.do_resolve = 1;
	bgn_
	/*
	   the mode will be needed by narrative_op
	*/
	in_( "!. %[" )			restore_mode = context->expression.mode;
	in_( ": identifier : !. %[" )	restore_mode = context->expression.mode;
	/*
	   in loop or condition evaluation we do not want expression_solve() to
	   resolve filtered results if there are, and unless specified otherwise.
	   Note that expression_solve may revert to EvaluateMode.
	*/
	in_( "? identifier:" )		context->expression.do_resolve = 0;
	in_( "? %:" )			context->expression.do_resolve = 0;
	in_( "?~ %:" )			context->expression.do_resolve = 0;
	in_( ": identifier : %[" )	context->expression.do_resolve = 0;	// cf. assign_results, assign_va, assign_narrative
	in_( ">: %[" )			context->expression.do_resolve = 0;	// cf. output_results, output_va, output_narrative
					context->error.flush_output = 1;	// we want to output errors in the text
	end

	do_( parse_expression, same );
	if ( context->expression.mode != ErrorMode ) {
		Expression *expression = context->expression.ptr;
		context->expression.mode = EvaluateMode;
		int retval = expression_solve( expression, 3, context );
		if ( retval < 0 ) event = retval;
	}
	context->expression.do_resolve = 0;

	if ( restore_mode ) {
		context->expression.mode = restore_mode;
	}
	return event;
}

int
read_expression( char *state, int event, char **next_state, _context *context )
{
	if ( context_check( FreezeMode, 0, 0 ) ) {
		do_( flush_input, same );
		return event;
	}

	// the mode may be needed by expression_op
	int restore_mode = context->expression.mode;
	do_( parse_expression, same );
	context->expression.mode = restore_mode;

	return event;
}

/*---------------------------------------------------------------------------
	command narrative actions
---------------------------------------------------------------------------*/
int
overwrite_narrative( Entity *e, _context *context )
{
	char *name = context->identifier.id[ 1 ].ptr;

	// does e already have a narrative with the same name?
	Narrative *n = lookupNarrative( e, name );
	if ( n == NULL ) return 1;

	// if yes, then is that narrative already active?
	else if ( lookupByAddress( n->instances, e ) != NULL )
	{
		if ( e == CN.nil ) {
			fprintf( stderr, "consensus> Warning: narrative '%s()' is already active - "
				"cannot overwrite\n", name );
		} else {
			fprintf( stderr, "consensus> Warning: narrative %%'" ); output_name( e, NULL, 0 );
			fprintf( stderr, ".%s()' is already active - cannot overwrite\n", name );
		}
		return 0;
	}
	else	// let's talk...
	{
		int overwrite = 0;
		if ( e == CN.nil ) {
			fprintf( stderr, "consensus> Warning: narrative '%s()' already exists. ", name );
		} else {
			fprintf( stderr, "Consensus> Warning: narrative %%'" ); output_name( e, NULL, 0 );
			fprintf( stderr, ".%s()' already exists. ", name );
		}
		do {
			fprintf( stderr, "Overwrite? (y/n)_ " );
			overwrite = getchar();
			switch ( overwrite ) {
			case 'y':
			case 'n':
				if ( getchar() == '\n' )
					break;
			default:
				overwrite = 0;
				while ( getchar() != '\n' );
				break;
			}
		}
		while ( !overwrite );
		if ( overwrite == 'n' )
			return 0;

		removeNarrative( e, n );
	}
	return 1;
}

int
narrative_op( char *state, int event, char **next_state, _context *context )
{
	if ( !context_check( 0, 0, ExecutionMode ) )
		return 0;

	switch ( context->expression.mode ) {
	case InstantiateMode:
		; listItem *last_i = NULL, *next_i;
		for ( listItem *i = context->expression.results; i!=NULL; i=i->next ) {
			next_i = i->next;
			if ( !overwrite_narrative(( Entity *) i->ptr, context ) )
			{
				clipListItem( &context->expression.results, i, last_i, next_i );
			}
			else last_i = i;
		}
		read_narrative( state, event, next_state, context );
		Narrative *narrative = context->narrative.current;
		if ( narrative == NULL ) {
			output( Warning, "narrative empty - not instantiated" );
			return 0;
		}
		int do_register = 0;
		for ( listItem *i = context->expression.results; i!=NULL; i=i->next )
		{
			Entity *e = (Entity *) i->ptr;
			if ( cn_instantiate_narrative( e, narrative ) )
				do_register = 1;
		}
		if ( do_register ) {
			registerNarrative( narrative, context );
			output( Info, "narrative instantiated: %s()", narrative->name );
		} else {
			freeNarrative( narrative );
			output( Warning, "no target entity - narrative not instantiated" );
		}
		context->narrative.current = NULL;
		break;

	case ReleaseMode:
		; char *name = context->identifier.id[ 1 ].ptr;
		for ( listItem *i = context->expression.results; i!=NULL; i=i->next )
		{
			Entity *e = (Entity *) i->ptr;
			cn_release_narrative( e, name );
		}
		event = 0;
		break;

	case ActivateMode:
		name = context->identifier.id[ 1 ].ptr;
		for ( listItem *i = context->expression.results; i!=NULL; i=i->next )
		{
			Entity *e = (Entity *) i->ptr;
			cn_activate_narrative( e, name );
		}
		event = 0;
		break;

	case DeactivateMode:
		name = context->identifier.id[ 1 ].ptr;
		for ( listItem *i = context->expression.results; i!=NULL; i=i->next )
		{
			Entity *e = (Entity *) i->ptr;
			cn_deactivate_narrative( e, name );
		}
		event = 0;
		break;

	default:
		// only supports these modes for now...
		event = 0;
		break;
	}
	return 0;
}

int
exit_narrative( char *state, int event, char **next_state, _context *context )
{
	switch ( context->control.mode ) {
	case FreezeMode:
		return 0;
	case InstructionMode:
		if ( context->narrative.mode.action.one )
			break;
	case ExecutionMode:
		if ( context->narrative.current == NULL ) {
			return output( Error, "'exit' command only supported in narrative mode" );
		}
		else if ( context->narrative.mode.action.block && ( context->control.mode == InstructionMode ) ) {
			return output( Error, "'exit' is not a supported instruction - use 'do exit' action instead" );
		}
		context->narrative.current->deactivate = 1;
	}
	return event;
}

/*---------------------------------------------------------------------------
	command va actions
---------------------------------------------------------------------------*/
int
read_va( char *state, int event, char **next_state, _context *context )
{
	event = 0;	// we know it is '$'
	state = base;
	do {
	event = input( state, event, NULL, context );
#ifdef DEBUG
	output( Debug, "read_va: in \"%s\", on '%c'", state, event );
#endif
	bgn_
	in_( base ) bgn_
		on_( '(' )	do_( nop, "(" )
		on_other	do_( error, "" )
		end
		in_( "(" ) bgn_
			on_( ' ' )	do_( nop, same )
			on_( '\t' )	do_( nop, same )
			on_other	do_( read_2, "(_" )
			end
			in_( "(_" ) bgn_
				on_( ' ' )	do_( nop, same )
				on_( '\t' )	do_( nop, same )
				on_( ')' )	do_( nop, "" )
				on_other	do_( error, "" )
				end
	end
	}
	while ( strcmp( state, "" ) );

	return event;
}

/*---------------------------------------------------------------------------
	command input actions
---------------------------------------------------------------------------*/
int
input_command( char *state, int event, char **next_state, _context *context )
{
	switch ( context->control.mode ) {
	case FreezeMode:
		break;
	case InstructionMode:
		// sets execution flag so that the last instruction is parsed again,
		// but this time in execution mode
		context->control.mode = ExecutionMode;
		return push_input( NULL, NULL, LastInstruction, context );
	case ExecutionMode:
		break;
	}
	if ( context->identifier.id[ 0 ].type != StringIdentifier ) {
		return output( Error, "expected argument in \"quotes\"" );
	}
	char *identifier = string_extract( context->identifier.id[ 0 ].ptr );
	return push_input( identifier, NULL, PipeInput, context );
}

/*---------------------------------------------------------------------------
	command output actions
---------------------------------------------------------------------------*/
int
set_assignment_mode( char *state, int event, char **next_state, _context *context )
{
	bgn_
	in_( ">_" )	context->assignment.mode = AssignmentAdd;
	in_other	context->assignment.mode = AssignmentSet;
	end
	return 0;
}
