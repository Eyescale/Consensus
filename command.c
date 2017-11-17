#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "database.h"
#include "registry.h"
#include "kernel.h"

#include "command.h"
#include "command_util.h"
#include "io.h"
#include "frame.h"
#include "input.h"
#include "input_util.h"
#include "output.h"
#include "path.h"
#include "expression.h"
#include "expression_util.h"
#include "variable.h"
#include "variable_util.h"
#include "value.h"

// #define DEBUG

static char *expression_clause = "expression clause";
static char *variable_clause = "variable clause";
static char *loop = "loop";

static _action set_expression_clause;
static _action set_variable_clause;
static _action set_loop;

/*---------------------------------------------------------------------------
	clause actions
---------------------------------------------------------------------------*/
static int
set_clause_to_contrary( char *state, int event, char **next_state, _context *context )
{
	if ( !command_mode( 0, 0, ExecutionMode ) )
		return 0;

	context->command.contrary = ~context->command.contrary;
	return 0;
}

static int
set_clause( int clause, char *state, int event, char **next_state, _context *context )
{
	switch ( context->command.mode ) {
	case FreezeMode:
		break;
	case InstructionMode:
		; StackVA *stack = (StackVA *) context->command.stack->ptr;
		stack->clause.state = ActiveClause;
		break;
	case ExecutionMode:
		if ( context->expression.mode == ReadMode ) {
			freeLiteral( &context->expression.results );
		}
		stack = (StackVA *) context->command.stack->ptr;
		stack->clause.state = clause ?
			( context->command.contrary ? PassiveClause : ActiveClause ) :
			( context->command.contrary ? ActiveClause : PassiveClause );
		if ( stack->clause.state == PassiveClause )
			set_command_mode( FreezeMode, context );
	}
	context->command.contrary = 0;
	return 0;
}

static int
set_variable_clause( char *state, int event, char **next_state, _context *context )
{
	int clause = ( lookupVariable( context, NULL, 0 ) != NULL );
	return set_clause( clause, state, event, next_state, context );
}

static int
set_expression_clause( char *state, int event, char **next_state, _context *context )
{
	int clause = ( context->expression.results != NULL );
	return set_clause( clause, state, event, next_state, context );
}

static int
flip_clause( char *state, int event, char **next_state, _context *context )
{
	StackVA *stack = (StackVA *) context->command.stack->ptr;
	switch ( context->command.mode ) {
	case FreezeMode:
		if ( context->command.level == context->command.freeze.level )
		{
			stack->clause.state = ActiveClause;
			set_command_mode( ExecutionMode, context );
			set_record_mode( OffRecordMode, event, context );
		}
		break;
	case InstructionMode:
		switch ( stack->clause.state ) {
			case ActiveClause:
			case PassiveClause:
				break;
			case ClauseNone:
				return output( Error, "not in conditional execution mode" );
		}
		break;
	case ExecutionMode:
		switch ( stack->clause.state ) {
			case ActiveClause:
				stack->clause.state = PassiveClause;
				set_command_mode( FreezeMode, context );
				break;
			case PassiveClause:
				stack->clause.state = ActiveClause;
				break;
			case ClauseNone:
				return output( Error, "not in conditional execution mode" );
		}
	}
	return 0;
}

/*---------------------------------------------------------------------------
	loop actions
---------------------------------------------------------------------------*/
static int
set_loop( char *state, int event, char **next_state, _context *context )
{
	if ( !command_mode( 0, 0, ExecutionMode ) )
		return 0;

	StackVA *stack = (StackVA *) context->command.stack->ptr;
	if ( context->input.stack == NULL ) {
		// here we are reading instructions from stdin
		set_command_mode( InstructionMode, context );
		set_record_mode( RecordInstructionMode, event, context );
	}
	else {
		InputVA *input = (InputVA *) context->input.stack->ptr;
		if ( input->mode == InstructionBlock ) {
			// here we are about to start a loop within a loop
			stack->loop.begin = context->input.instruction->next;
		}
		else {
			// here we may be reading from a script
			set_command_mode( InstructionMode, context );
			set_record_mode( RecordInstructionMode, event, context );
		}
	}
	if ( context->expression.results == NULL ) {
		set_command_mode( FreezeMode, context );
	}
	else {
		stack->loop.candidates = context->expression.results;
		context->expression.results = NULL;
		int type = (( context->expression.mode == ReadMode ) ? LiteralVariable : EntityVariable );
		assign_variable( &variator_symbol, stack->loop.candidates->ptr, type, context );
	}
	return 0;
}

/*---------------------------------------------------------------------------
	command_push
---------------------------------------------------------------------------*/
static int
command_push( char *state, int event, char **next_state, _context *context )
{
#ifdef DEBUG
	outputf( Debug, "parser: command: pushing from level %d", context->command.level );
#endif
	// special case: pushing a loop in interactive mode, but no result to loop over
	if ( command_mode( 0, 0, ExecutionMode ) && ( context->input.stack == NULL ) &&
	     !strcmp( *next_state, loop ) && ( context->expression.results == NULL ))
	{
		*next_state = same; // nothing to push for in this case
	}
	else {
		event = push( state, event, &base, context );
		if ( !strcmp( *next_state, loop ) ) {
			event = set_loop( state, event, next_state, context );
		}
		else if ( !strcmp( *next_state, expression_clause ) ) {
			event = set_expression_clause( state, event, next_state, context );
		}
		else if ( !strcmp( *next_state, variable_clause ) ) {
			event = set_variable_clause( state, event, next_state, context );
		}
		*next_state = base;
	}
	return event;
}

/*---------------------------------------------------------------------------
	pop_narrative
---------------------------------------------------------------------------*/
static int
pop_narrative( char *state, int event, char **next_state, _context *context )
{
	switch ( context->command.mode ) {
	case FreezeMode:
		break;
	case InstructionMode:
		if ( context->command.level != context->record.level )
			break;
		if ( context->record.instructions == NULL )
			break;	// not recording - most likely replaying
		if ( !strcmp( state, "/." ) ) {
			if ( context->record.instructions->next == NULL ) {
				output( Warning, "no action specified - will be removed from narrative..." );
				freeInstructionBlock( context );
			}
		}
		else if ( !strcmp( state, "/" ) ) {
			if ( context->record.instructions->next == NULL ) {
				free ( context->record.instructions->ptr );
				context->record.instructions->ptr = strdup( "~." );
			}
			*next_state = "";
			return 0;
		}
		else
			return outputf( Debug, "***** Error: narrative_pop: unexpected state '%s'", state );
		break;
	case ExecutionMode:
		if ( context->command.level != context->narrative.level )
			break;
		if ( !strcmp( state, "/" ) ) {
			*next_state = "";
			return 0;
		}
		break;
	}

	event = pop( state, event, next_state, context );
	if ( context->command.mode == FreezeMode )
		*next_state = base;
	else if ( context->command.level < context->record.level )
		// popping from narrative instruction block - return control to narrative
		*next_state = "";

	return event;
}

static int
command_pop( char *state, int event, char **next_state, _context *context )
{
	if ( context->command.block )
		return pop_narrative( state, event, next_state, context );

	// popping from loop or clause
	// ---------------------------
	StackVA *stack = (StackVA *) context->command.stack->ptr;

	switch ( context->command.mode ) {
	case FreezeMode:
		event = pop( state, event, next_state, context );
		*next_state = base;
		return event;
	case InstructionMode:
		if ( context->record.instructions == NULL )
			break;	// not recording - most likely replaying
		if ( context->command.level != context->record.level )
			break;

		// popping from loop instruction mode - launches execution
		// -------------------------------------------------------
		set_command_mode( ExecutionMode, context );
		set_record_mode( OffRecordMode, event, context );
		// check that we do actually have recorded instructions to execute besides '/'
		if (( context->record.instructions->next )) {
			reorderListItem( &context->record.instructions );
			stack->loop.begin = context->record.instructions;
			push_input( "", context->record.instructions, InstructionBlock, context );

			*next_state = base;
			return 0;
		}
		freeListItem( &stack->loop.candidates );
		freeInstructionBlock( context );
		break;
	case ExecutionMode:
		if ( stack->loop.begin == NULL )
			break;	// not in a loop

		// popping during loop execution
		// -----------------------------
		if (( stack->loop.candidates->next )) {
			popListItem( &stack->loop.candidates );
			assign_variable( &variator_symbol, stack->loop.candidates->ptr, 0, context );
			set_instruction( stack->loop.begin, context );
			*next_state = base;
			return 0;
		}
		freeListItem( &stack->loop.candidates );
		if ( context->command.level == context->record.level ) {
			pop_input( state, event, next_state, context );
			freeInstructionBlock( context );
		}
		else set_instruction( context->input.instruction->next, context );
		break;
	}

	event = pop( state, event, next_state, context );
	return event;
}

/*---------------------------------------------------------------------------
	command_err
---------------------------------------------------------------------------*/
static int
command_err( char *state, int event, char **next_state, _context *context )
{
	event = clear_output_target( state, event, next_state, context );
	if ( context->command.one ||
	     ( context->narrative.mode.editing && (context->input.stack) ))
	{
		event = error( state, event, next_state, context );
		*next_state = out;
	}
	else {
		error( state, event, next_state, context );
		context->error.tell = 1;
		*next_state = base;
		event = 0;
	}
	return event;
}

/*---------------------------------------------------------------------------
	command_pop_input
---------------------------------------------------------------------------*/
static int
command_pop_input( char *state, int event, char **next_state, _context *context )
{
	if ((context->input.stack))
	{
		InputVA *input = context->input.stack->ptr;
		int mode = input->mode;

		event = pop_input( state, event, next_state, context );

		switch ( mode ) {
		case HCNFileInput:
		case HCNValueFileInput:
			*next_state = "";
			break;
		case InstructionInline:
			set_command_mode( InstructionMode, context );
			set_record_mode( RecordInstructionMode, event, context );
			break;
		default:
			break;
		}
	}
	else *next_state = "";	// return from UNIX command pipe

	return event;
}

/*---------------------------------------------------------------------------
	command_read
---------------------------------------------------------------------------*/
int
command_read( void *src, InputType type, char *state, int event, _context *context )
{
	if ( event < 0 ) return event;

	// push input
	// ---------------------------
	int restore[ 3 ] = { 0, 0, 0 };
	restore[ 0 ] = context->input.level;
	restore[ 1 ] = context->command.one;
	restore[ 2 ] = context->command.block;

	int retval = ((src) ? push_input( "", src, type, context ) : 0 );
	if ( retval < 0 ) return retval;

	// process input
	// ------------------------------
	context->command.one = ( type == InstructionOne );
	context->command.block = ( type == InstructionBlock );
	do {
		event = io_scan( state, event, &same, context );
		event = read_command( state, event, &state, context );
		event = frame_update( state, event, &same, context );
	}
	while ( strcmp( state, "" ) );

	// pop input
	// ------------------------------
	while ( context->input.level > restore[ 0 ] )
		pop_input( base, 0, &same, context );
	context->command.one = restore[ 1 ];
	context->command.block = restore[ 2 ];

	return event;
}

/*---------------------------------------------------------------------------
	check_out	- local
---------------------------------------------------------------------------*/
static int
check_out( char *state, int event, char **next_state, _context *context )
{
	if ( !strcmp( state, out ) ) {
		event = clear_output_target( state, event, next_state, context );
		*next_state = (( context->command.one ) ? "" : base );
	}
	else *next_state = same;

	return event;
}

/*---------------------------------------------------------------------------
	read_command
---------------------------------------------------------------------------*/
#define do_( a, s ) \
	{ addItem( &actions, &a ); addItem( &states, s ); }

#define BRKOUT if ( context->command.one ) do_( nothing, "" ) else
#define BRKERR if ( context->command.one ) do_( error, "" ) else
/*
	The user command
		>@session:< whatever
	requires the sender to keep the socket connection opened until
	the response is sent (otherwise the receiver will crash writing
	on a closed socket connection).
	To achieve thispurpose, it translates internally into:
		>@session:>..: whatever |
	which requires the receiver to write back the response, while
	blocking the sender until the response is sent.
	The following macro is used to prevent the user from issueing
	the internal command above without sync (|)
*/
#define	SAME_OR( s )	((context->output.query && !context->output.marked) ? s : same )

int
read_command( char *state, int event, char **next_state, _context *context )
{
	listItem *actions = NULL;
	listItem *states = NULL;

	// ---------------------------------------------------------------
	// 1. input event
	// ---------------------------------------------------------------

	event = read_input( state, event, NULL, context );

#ifdef DEBUG
	outputf( Debug, "read_command: state=\"%s\", event='%c'", state, event );
#endif

	// ---------------------------------------------------------------
	// 2. translate event into actions and target states
	// ---------------------------------------------------------------

	switch ( event ) {
	case 0: // input() reached EOF
		do_( command_pop_input, base )
		break;
	case -1:
		do_( command_err, base )
		break;
	case ' ':
	case '\t':
		// in general, we want to ignore ' ' and '\t'
		// but there are special cases...

		switch( state[ strlen(state) - 1 ] ) {
		case '%':
			do_( command_err, base )
			break;
		default:
			bgn_
			in_( ">:" )			do_( output_char, same )
			in_( ">: \\" )			do_( output_special_char, ">:" )
			in_( ">: \"" )			do_( output_char, same )
			in_( ">: \"\\" )		do_( output_special_char, ">: \"" )
			in_( ">: %?" )			do_( output_variator_value, ">:" )
			in_( ">: %variable" )		do_( output_variable_value, ">:" )
			in_( ">: %<_>" )		do_( output_scheme_descriptor, ">:" )
			in_( ">: %<_>.$(_)" )		do_( output_scheme_va, ">:" )
			in_( ">: %[_]" )		do_( output_results, ">:" )
			in_( ">: %[_].$(_)" )		do_( output_va, ">:" )
			in_( ">: %[_].narrative(_)" )	do_( output_narrative, ">:" )
			in_other			do_( nop, same )
			end
		}
		break;
	default:
		// special events 0, -1, ' ', '\t' have been handled above
		// other events are handled here, depending on state:

		bgn_
		in_( base ) bgn_
			on_( '\n' )	do_( nop, same )
			on_( '|' )	do_( sync_delivery, same )
			on_( ':' )	do_( nop, ":" )
			on_( '>' )	do_( nop, ">" )
			on_( '<' )	do_( nop, "<" )
			on_( '!' )	do_( nop, "!" )
			on_( '~' )	do_( nop, "~" )
			on_( '?' )	BRKERR do_( nop, "?" )
			on_( '/' )	BRKERR do_( nop, "/" )
			on_( 'e' )	do_( read_token, "exit\0exit" )
			on_other	do_( command_err, base )
			end

		in_( "<" ) bgn_
			on_( 'c' )	do_( read_token, "cgi\0< cgi" )
			on_( 's' )	do_( read_token, "session:\0< session:" )
			on_other	do_( command_err, base )
			end
			in_( "< cgi" ) bgn_
				on_( '>' )	do_( nop, "< cgi >" )
				on_other	do_( command_err, base )
				end
				in_( "< cgi >" ) bgn_
					on_( ':' )	do_( input_cgi, "< cgi >:" )
					on_other	do_( command_err, base )
					end
			in_("< session:" ) bgn_
				on_any	do_( read_path, "< session://path" )
				end
				in_("< session://path" ) bgn_
					on_( '>' )	do_( nop, "< session://path >" )
					on_other	do_( command_err, base )
					end
				in_("< session://path >" ) bgn_
					on_( ':' )	do_( nop, "< session://path >:" )
					on_other	do_( command_err, base )
					end
				in_("< session://path >:" ) bgn_
					on_any	do_( read_2, "< session://path >: event" )
					end
				in_("< session://path >: event" ) bgn_
					on_( '@' )	do_( nop, "< session://path >: event @" )
					on_other	do_( command_err, base )
					end
				in_("< session://path >: event @" ) bgn_
					on_( '\n' )	do_( handle_iam, base )
					on_other	do_( command_err, base )
					end

		in_( "exit" ) bgn_
			on_( '\n' )	do_( exit_command, out )
			on_other	do_( command_err, base )
			end

		in_( "/" ) bgn_
			on_( '\n' )	do_( command_pop, pop_state )
			on_( '~' )	do_( nop, "/~" )
			on_( '.' )	do_( nop, "/." )
			on_other	do_( command_err, base )
			end
			in_( "/~" ) bgn_
				on_( '\n' )	do_( flip_clause, base )
				on_other	do_( command_err, base )
				end
			in_( "/." ) bgn_
				on_( '\n' )	do_( command_pop, pop_state )
				on_other	do_( command_err, base )
				end

		in_( "!" ) bgn_
			on_( '!' )	do_( set_command_op, "!." )
			on_( '~' )	do_( set_command_op, "!." )
			on_( '*' )	do_( set_command_op, "!." )
			on_( '_' )	do_( set_command_op, "!." )
			on_other	do_( command_err, base )
			end
			in_( "!." ) bgn_
				on_( '<' )	do_( nop, "!. <" )
				on_other	do_( read_expression, "!. expression" )
				end
			in_( "!. <" ) bgn_
				on_any		do_( read_scheme, "!. < scheme" )
				end
				in_( "!. < scheme" ) bgn_
					on_( ':' )	do_( nop, "!. < scheme:" )
					on_( '>' )	do_( nop, "!. <_>" )
					on_other	do_( command_err, base )
					end
				in_( "!. < scheme:" ) bgn_
					on_any	do_( read_path, "!. <_" )
					end
				in_( "!. <_" ) bgn_
					on_( '>' )	do_( nop, "!. <_>" )
					on_other	do_( command_err, base )
					end
				in_( "!. <_>" ) bgn_
					on_( '\n' )	do_( scheme_op, out )
					on_other	BRKOUT do_( command_err, base )
					end
			in_( "!. expression" ) bgn_
				on_( '\n' )	do_( expression_op, out )
				on_( '(' )	do_( nop, "!. %[_].narrative(" )
				on_other	BRKOUT do_( command_err, base )
				end
				in_( "!. %[_].narrative(" ) bgn_
					on_( ')' )	do_( nop, "!. %[_].narrative(_)" )
					on_other	do_( command_err, base )
					end
					in_( "!. %[_].narrative(_)" ) bgn_
						on_( '\n' )	do_( narrative_op, out )
						on_other	BRKOUT do_( command_err, base )
						end

		in_( "~" ) bgn_
			on_( '.' )	do_( nop, "~." )
			on_other	do_( command_err, base )
			end
			in_( "~." ) bgn_
				on_( '\n' )	do_( nop, out )
				on_other	BRKOUT do_( command_err, base )
				end

		in_( ">" ) bgn_
			on_( '@' )	do_( nop, ">@" )
			on_( ':' )	do_( set_output_target, ">:" )
			on_( '.' )	do_( nop, "> ." )
			on_other	do_( read_0, "> identifier" )
			end
			in_( ">@" ) bgn_
				on_( '%' )	do_( nop, ">@ %" )
				on_other	do_( read_path, ">@session" )
				end
				in_( ">@ %" ) bgn_
					on_any	do_( read_0, ">@ %session" )
					end
					in_( ">@ %session" ) bgn_
						on_( ':' )	do_( set_output_from_variable, ">@ %session:" )
						on_other	do_( command_err, base )
						end
					in_( ">@ %session:" ) bgn_
						on_( '\n' )	do_( nop, out )
						on_other	do_( nothing, ">:" )
						end
				in_( ">@session" ) bgn_
					on_( ':' )	do_( set_output_target, ">:" )
					on_other	do_( command_err, base )
					end
			in_( "> ." ) bgn_
				on_( '.' )	do_( nop, "> .." )
				on_other	do_( command_err, base )
				end
				in_( "> .." ) bgn_
					on_( ':' )	do_( set_output_target, ">:" )
					on_other	do_( command_err, base )
					end
			in_( "> identifier" ) bgn_
				on_( ':' )	do_( set_assignment_mode, ": identifier :" )
				on_other	do_( command_err, base )
				end

		in_( ">:" ) bgn_
			on_( '\n' )	do_( output_char, out )
			on_( '|' )	do_( sync_output, ">:_ |" )
			on_( '\\' )	do_( nop, ">: \\" )
			on_( '%' )	do_( nop, ">: %" )
			on_( '"' )	do_( nop, ">: \"" )
			on_( '<' )	do_( backfeed_output, same )
			on_( '>' )	do_( output_char, SAME_OR( ">: >" ) )
			on_other	do_( output_char, same )
			end
			in_( ">: >" ) bgn_
				on_( '.' )	do_( command_err, base )
				on_other	do_( output_char, ">:" )
				end
			in_( ">: \\" ) bgn_
				on_( '\n' )	do_( nop, ">:" )
				on_other	do_( output_special_char, ">:" )
				end
			in_( ">: \"" ) bgn_
				on_( '\n' )	do_( nop, same )
				on_( '"' )	do_( nop, ">:" )
				on_( '\\' )	do_( nop, ">: \"\\" )
				on_( '<' )	do_( backfeed_output, same )
				on_( '>' )	do_( output_char, SAME_OR( ">: \">" ) )
				on_other	do_( output_char, same )
				end
				in_( ">: \">" ) bgn_
					on_( '.' )	do_( command_err, base )
					on_other	do_( output_char, ">: \"" )
					end
				in_( ">: \"\\" ) bgn_
					on_( '\n' )	do_( nop, ">: \"" )
					on_other	do_( output_special_char, ">: \"" )
					end
			in_( ">: %" ) bgn_
				on_( '?' )	do_( nop, ">: %?" )
				on_( '<' )	do_( nop, ">: %<" )
				on_( '[' )	do_( nop, ">: %[" )
				on_( '.' )	do_( set_results_to_nil, ">: %[_]." )
				on_( '\n' )	do_( output_mod, ">:" )
				on_separator	do_( output_mod, ">:" )
				on_other	do_( read_1, ">: %variable" )
				end
				in_( ">: %?" ) bgn_
					on_any	do_( output_variator_value, ">:" )
					end
				in_( ">: %variable" ) bgn_
					on_any	do_( output_variable_value, ">:" )
					end
				in_( ">: %<" ) bgn_
					on_any	do_( read_scheme, ">: %< scheme" )
					end
					in_( ">: %< scheme" ) bgn_
						on_( ':' )	do_( nop, ">: %< scheme:" )
						on_( '>' )	do_( nop, ">: %<_>" )
						on_other	do_( command_err, base )
						end
					in_( ">: %< scheme:" ) bgn_
						on_any	do_( read_path, ">: %<_" )
						end
						in_( ">: %<_" ) bgn_
							on_( '>' )	do_( nop, ">: %<_>" )
							on_other	do_( command_err, base )
							end
					in_( ">: %<_>" ) bgn_
						on_( '.' )	do_( nop, ">: %<_>." )
						on_other	do_( output_scheme_descriptor, ">:" )
						end
						in_( ">: %<_>." ) bgn_
							on_( '$' )	do_( nop, ">: %<_>.$" )
							on_other	do_( command_err, base )
							end
						in_( ">: %<_>.$" ) bgn_
							on_( '(' )	do_( nop, ">: %<_>.$(" )
							on_other	do_( command_err, base )
							end
						in_( ">: %<_>.$(" ) bgn_
							on_any	do_( read_2, ">: %<_>.$(_" )
							end
						in_( ">: %<_>.$(_" ) bgn_
							on_( ')' )	do_( nop, ">: %<_>.$(_)" )
							end
					in_( ">: %<_>.$(_)" ) bgn_
						on_any	do_( output_scheme_va, ">:" )
						end
				in_( ">: %[" ) bgn_
					on_any	do_( evaluate_expression, ">: %[_" )
					end
					in_( ">: %[_" ) bgn_
						on_( ']' )	do_( nop, ">: %[_]")
						on_other	do_( command_err, base )
						end
						in_( ">: %[_]" ) bgn_
							on_( '.' )	do_( nop, ">: %[_]." )
							on_other	do_( output_results, ">:" )
							end
							in_( ">: %[_]." ) bgn_
								on_( '$' )	do_( nop, ">: %[_].$" )
								on_other	do_( read_1, ">: %[_].narrative" )
								end
								in_( ">: %[_].$" ) bgn_
									on_( '(' )	do_( nop, ">: %[_].$(" )
									on_other	do_( command_err, base )
									end
								in_( ">: %[_].$(" ) bgn_
									on_any		do_( read_2, ">: %[_].$(_" )
									end
								in_( ">: %[_].$(_" ) bgn_
									on_( ')' )	do_( nop, ">: %[_].$(_)" )
									on_other	do_( command_err, base )
									end
							in_( ">: %[_].$(_)" ) bgn_
								on_any	do_( output_va, ">:" )
								end
							in_( ">: %[_].narrative" ) bgn_
								on_( '(' )	do_( nop, ">: %[_].narrative(" )
								on_other	do_( command_err, base )
								end
								in_( ">: %[_].narrative(" ) bgn_
									on_( ')' )	do_( nop, ">: %[_].narrative(_)" )
									on_other	do_( command_err, base )
									end
								in_( ">: %[_].narrative(_)" ) bgn_
									on_any	do_( output_narrative, ">:" )
									end
			in_( ">:_ |" ) bgn_
				on_( ':' )	do_( nop, ">:_ | :" );
				on_( '>' )	do_( nop, ">:_ | >" );
				on_other	do_( sync_output, out )
				end
				in_( ">:_ | :" ) bgn_
					on_( '<' )	do_( pipe_output_in, ">:_ | :<" )
					on_other	do_( command_err, base )
					end
					in_( ">:_ | :<" ) bgn_
						on_( '\n' )	do_( nop, out )
						on_other	do_( nothing, out )
						end
				in_( ">:_ | >" ) bgn_
					on_( ':' )	do_( pipe_output_out, ">:_ | >:" )
					on_other	do_( command_err, base )
					end
					in_( ">:_ | >:" ) bgn_
						on_( '\n' )	do_( nop, out )
						on_other	do_( nothing, out )
						end

		in_( "?" ) bgn_
			on_( '%' )	do_( nop, "? %" )
			on_( '~' )	do_( nop, "?~" )
			on_( '\n' )	do_( command_err, base )
			on_( ':' )	do_( nop, "? identifier:" )
			on_other	do_( read_0, "? identifier" )
			end
			in_( "? %" ) bgn_
				on_( ':' )	do_( nop, "? %:" )
				on_( '[' )	do_( nop, "? %[" )
				on_other	do_( read_1, "? %variable" )
				end
				in_( "? %:" ) bgn_
					on_( '\n' )	do_( command_err, base )
					on_other	do_( evaluate_expression, "? %: expression" )
					end
					in_( "? %: expression" ) bgn_
						on_( '\n' )	do_( command_push, expression_clause )
						on_other	do_( command_err, base )
						end
				in_( "? %[" ) bgn_
					on_any	do_( evaluate_expression, "? %[_" )
					end
					in_( "? %[_" ) bgn_
						on_( ']' )	do_( nop, "? %[_]" )
						on_other	do_( command_err, base )
						end
						in_( "? %[_]" ) bgn_
							on_( '\n' )	do_( command_push, expression_clause )
							on_other	do_( command_err, base )
							end
				in_( "? %variable" ) bgn_
					on_( '\n' )	do_( command_push, variable_clause )
					on_other	do_( command_err, base )
					end

			in_( "?~" ) bgn_
				on_( '%' )	do_( nop, "?~ %" )
				on_other	do_( command_err, base )
				end
				in_( "?~ %" ) bgn_
					on_( ':' )	do_( nop, "?~ %:" )
					on_( '[' )	do_( nop, "?~ %[" )
					on_other	do_( read_1, "?~ %variable" )
					end
					in_( "?~ %:" ) bgn_
						on_( '\n' )	do_( command_err, base )
						on_other	do_( evaluate_expression, "?~ %: expression" )
						end
						in_( "?~ %: expression" ) bgn_
							on_( '\n' )	do_( set_clause_to_contrary, same )
									do_( command_push, expression_clause )
							on_other	do_( command_err, base )
							end
		
					in_( "?~ %[" ) bgn_
						on_any	do_( evaluate_expression, "?~ %[_" )
						end
						in_( "?~ %[_" ) bgn_
							on_( ']' )	do_( nop, "?~ %[_]" )
							on_other	do_( command_err, base )
							end
							in_( "?~ %[_]" ) bgn_
								on_( '\n' )	do_( set_clause_to_contrary, same )
										do_( command_push, expression_clause )
								on_other	do_( command_err, base )
										end
					in_( "?~ %variable" ) bgn_
						on_( '\n' )	do_( set_clause_to_contrary, same )
								do_( command_push, variable_clause )
						on_other	do_( command_err, base )
						end

			in_( "? identifier" ) bgn_
				on_( '\n' )	do_( command_err, base )
				on_( ':' )	do_( nop, "? identifier:" )
				on_other	do_( command_err, base )
				end
				in_( "? identifier:" ) bgn_
					on_( '%' )	do_( nop, "? identifier: %" )
					on_( '\n' )	do_( command_err, base )
					on_other	do_( evaluate_expression, "? identifier: expression" )
					end
					in_( "? identifier: expression" ) bgn_
						on_( '\n' )	do_( command_push, loop )
						on_other	do_( command_err, base )
						end
					in_( "? identifier: %" ) bgn_
						on_( '[' )	do_( nop, "? identifier: %[" )
						on_other	do_( command_err, base )
						end
						in_( "? identifier: %[" ) bgn_
							on_any	do_( evaluate_expression, "? identifier: %[_" )
							end
						in_( "? identifier: %[_" ) bgn_
							on_( ']' )	do_( nop, "? identifier: %[_]" )
							on_other	do_( command_err, base )
							end
						in_( "? identifier: %[_]" ) bgn_
							on_( '\n' )	do_( command_push, loop )
							on_other	do_( command_err, base )
							end
		in_( ":" ) bgn_
			on_( '\n' )	do_( command_err, base )
			on_( '%' )	do_( read_expression, ": expression" )
			on_( '<' )	do_( nop, ":<" )
			on_( '~' )	do_( nop, ":~" )
			on_other	do_( read_0, ": identifier" )
			end
			in_( ": identifier" ) bgn_
				on_( '<' )	do_( set_output_target, ">:" )
				on_( ':' )	do_( set_assignment_mode, ": identifier :" )
				on_other	do_( command_err, base )
				end

#if 1
		// does not support literal filters as do_resolve is not set -
		// best way to fix it is to rewrite entirely with variables as strings
		in_( ": identifier :" ) bgn_
			on_( '\n' )	do_( command_err, base )
			on_( '!' )	do_( nop, ": identifier : !" )
			on_other	do_( read_expression, ": identifier : expression" )
			end
			in_( ": identifier : expression" ) bgn_
				on_( '(' )	do_( nop, ": identifier : %[_].narrative(" )
				on_( '$' )	do_( nop, ": identifier : %[_].$" )
				on_( '\n' )	do_( assign_expression, out )
				on_other	BRKOUT do_( command_err, base )
				end
				in_( ": identifier : %[_].narrative(" ) bgn_
					on_( ')' )	do_( nop, ": identifier : %[_].narrative(_)" )
					on_other	do_( command_err, base )
					end
					in_( ": identifier : %[_].narrative(_)" ) bgn_
						on_( '\n' )	do_( assign_narrative, out )
						on_other	BRKOUT do_( command_err, base )
						end
				in_( ": identifier : %[_].$" ) bgn_
					on_( '(' )	do_( nop, ": identifier : %[_].$(" )
					on_other	do_( command_err, base )
					end
					in_( ": identifier : %[_].$(" ) bgn_
						on_any	do_( read_2, ": identifier : %[_].$(_" )
						end
						in_( ": identifier : %[_].$(_" ) bgn_
							on_( ')' )	do_( nop, ": identifier : %[_].$(_)" )
							on_other	do_( command_err, base )
							end
							in_( ": identifier : %[_].$(_)" ) bgn_
								on_( '\n' )	do_( assign_va, out )
								on_other	BRKOUT do_( command_err, base )
								end
#else
		in_( ": identifier :" ) bgn_
			on_( '%' )	do_( nop, ": identifier : %" )
			on_( '\n' )	do_( command_err, base )
			on_( '!' )	do_( nop, ": identifier : !" )
			on_other	do_( read_expression, ": identifier : expression" )
			end
			in_( ": identifier : expression" ) bgn_
				on_( '(' )	do_( set_results_to_nil, ": identifier : narrative(" )
				on_( '\n' )	do_( assign_expression, out )
				on_other	BRKOUT do_( command_err, base )
				end
			in_( ": identifier : %" ) bgn_
				on_( '[' )	do_( nop, ": identifier : %[" )
				on_other	do_( read_expression, ": identifier : expression" )
				end
				in_( ": identifier : %[" ) bgn_
					on_any	do_( evaluate_expression, ": identifier : %[_" )
					end
					in_( ": identifier : %[_" ) bgn_
						on_( ']' )	do_( nop, ": identifier : %[_]" )
						on_other	do_( command_err, base )
						end
					in_( ": identifier : %[_]" ) bgn_
						on_( '\n' )	do_( assign_results, out )
						on_( '.' )	do_( nop, ": identifier : %[_]." )
						on_other	BRKOUT do_( command_err, base )
						end
					in_( ": identifier : %[_]." ) bgn_
						on_( '$' )	do_( nop, ": identifier : %[_].$" )
						on_other	do_( read_1, ": identifier : %[_].narrative" )
						end
						in_( ": identifier : %[_].$" ) bgn_
							on_( '(' )	do_( nop, ": identifier : %[_].$(" )
							on_other	do_( command_err, base )
							end
						in_( ": identifier : %[_].$(" ) bgn_
							on_any		do_( read_2, ": identifier : %[_].$(_" )
							end
						in_( ": identifier : %[_].$(_" ) bgn_
							on_( ')' )	do_( nop, ": identifier : %[_].$(_)" )
							on_other	do_( command_err, base )
							end
					in_( ": identifier : %[_].$(_)" ) bgn_
						on_( '\n' )	do_( assign_va, out )
						on_other	do_( command_err, base )
						end
					in_( ": identifier : %[_].narrative" ) bgn_
						on_( '(' )	do_( nop, ": identifier : narrative(" )
						on_other	do_( command_err, base )
						end
						in_( ": identifier : narrative(" ) bgn_
							on_( ')' )	do_( nop, ": identifier : narrative(_)" )
							on_other	do_( command_err, base )
							end
						in_( ": identifier : narrative(_)" ) bgn_
							on_( '\n' )	do_( assign_narrative, out )
							on_other	BRKOUT do_( command_err, base )
							end
#endif
			in_( ": identifier : !" ) bgn_
				on_( '!' )	do_( set_command_op, ": identifier : !." )
				on_( '~' )	do_( set_command_op, ": identifier : !." )
				on_( '*' )	do_( set_command_op, ": identifier : !." )
				on_( '_' )	do_( set_command_op, ": identifier : !." )
				on_other	do_( command_err, base )
				end
				in_( ": identifier : !." ) bgn_
					on_any	do_( read_expression, ": identifier : !. expression" )
					end
					in_( ": identifier : !. expression" ) bgn_
						on_( '(' )	do_( nop, ": identifier : !. %[_].narrative(" )
						on_( '\n' )	do_( expression_op, same )
								do_( assign_results, out )
						on_other	BRKOUT do_( command_err, base )
						end
				in_( ": identifier : !. %[_].narrative(" ) bgn_
					on_( ')' )	do_( nop, ": identifier : !. %[_].narrative(_)" )
					on_other	do_( command_err, base )
					end
					in_( ": identifier : !. %[_].narrative(_)" ) bgn_
						on_( '\n' )	do_( narrative_op, same )
								do_( assign_narrative, out )
						on_other	BRKOUT do_( command_err, base )
						end

		in_( ": expression" ) bgn_
			on_( '$' )	do_( nop, ": %[_].$" )
			on_other	do_( command_err, base )
			end
			in_( ": %[_].$" ) bgn_
				on_( '(' )	do_( nop, ": %[_].$(" )
				on_other	do_( command_err, base )
				end
				in_( ": %[_].$(" ) bgn_
					on_any		do_( read_2, ": %[_].$(_" )
					end
				in_( ": %[_].$(_" ) bgn_
					on_( ')' )	do_( nop, ": %[_].$(_)" )
					on_other	do_( command_err, base )
					end
				in_( ": %[_].$(_)" ) bgn_
					on_( ':' )	do_( nop, ": %[_].$(_) :" )
					on_other	do_( command_err, base )
					end
			in_( ": %[_].$(_) :" ) bgn_
				on_( '%' )	do_( nop, ": %[_].$(_) : %" )
				on_other	do_( read_1, ": %[_].$(_) : arg" )
				end
				in_( ": %[_].$(_) : arg" ) bgn_
					on_( '\n' )	do_( set_va, out )
					on_( '(' )	do_( nop, ": %[_].$(_) : arg(" )
					on_other	BRKOUT do_( command_err, base )
					end
					in_( ": %[_].$(_) : arg(" ) bgn_
						on_( ')' )	do_( nop, ": %[_].$(_) : arg(_)" )
						on_other	do_( command_err, base )
						end
						in_( ": %[_].$(_) : arg(_)" ) bgn_
							on_( '\n' )	do_( set_va_from_narrative, out )
							on_other	BRKOUT do_( command_err, base )
							end
				in_( ": %[_].$(_) : %" ) bgn_
					on_any	do_( read_1, ": %[_].$(_) : %arg" )
					end
					in_( ": %[_].$(_) : %arg" ) bgn_
						on_( '\n' )	do_( set_va_from_variable, out )
						on_other	BRKOUT do_( command_err, base )
						end

		in_( ":~" ) bgn_
			on_( '\n' )	do_( reset_variables, out )
			on_other	do_( read_0, ":~ identifier" )
			end
			in_( ":~ identifier" ) bgn_
				on_( '\n' )	do_( variable_reset, out )
				on_other	BRKOUT do_( command_err, base )
				end

		in_( ":<" ) bgn_
			on_( 'f' )	do_( read_token, "file:\0:< file:" )
			on_( '%' )	do_( nop, ":< %" )
			on_( '\n' )	do_( nop, base )
			on_other	do_( command_err, base )
			end
			in_( ":< file:" ) bgn_
				on_any	do_( read_path, ":< file://path" )
				end
				in_( ":< file://path" ) bgn_
					on_( '\n' )	do_( input_story, base )
					on_other	BRKOUT do_( command_err, base )
					end
			in_( ":< %" ) bgn_
					on_( '(' )	do_( nop, ":< %(" )
					on_other	do_( command_err, base )
					end
				in_( ":< %(" ) bgn_
					on_other	do_( read_0, ":< %( string" )
					end
				in_( ":< %( string" ) bgn_
					on_( ')' )	do_( nop, ":< %( string )" )
					on_other	do_( command_err, base )
					end
				in_( ":< %( string )" ) bgn_
					on_( '\n' )	do_( input_inline, base )
					on_other	BRKOUT do_( command_err, base )
					end
		end
	}

	// -----------------------------------------------------------
	// 3. execute actions
	// -----------------------------------------------------------

	reorderListItem( &actions );
	reorderListItem( &states );
	for ( listItem *i=actions, *j=states; i!=NULL; i=i->next, j=j->next )
	{
		_action *action = i->ptr;
		*next_state = j->ptr;

		int retval = (*action)( state, event, next_state, context );
		if ( strcmp( *next_state, same ) ) state = *next_state;

		check_out( state, retval, next_state, context );
		if ( strcmp( *next_state, same ) ) state = *next_state;

		// In case the action did actually check out, ignore its
		// returned value - unless it's an error - and report the
		// triggering event back to the caller

		if ( !strcmp( state, "" ) ) {
			if ( retval < 0 ) event = retval;
			break;
		}
		event = retval;
	}
	freeListItem( &states );
	freeListItem( &actions );
	*next_state = state;
	return event;
}
