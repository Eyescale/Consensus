#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "database.h"
#include "registry.h"
#include "kernel.h"

#include "command.h"
#include "command_util.h"
#include "input.h"
#include "input_util.h"
#include "output.h"
#include "path.h"
#include "io.h"
#include "expression.h"
#include "variables.h"
#include "value.h"

// #define DEBUG

/*---------------------------------------------------------------------------
	execution engine
---------------------------------------------------------------------------*/
#define	do_( a, s ) \
	event = command_do_( a, &state, event, s, context );

static char *expression_clause = "expression clause";
static char *variable_clause = "variable clause";
static char *loop = "loop";

static _action set_expression_clause;
static _action set_variable_clause;
static _action set_loop;

static int
command_do_( _action action, char **state, int event, char *next_state, _context *context )
{
	int retval;

	if ( action == push )
	{
#ifdef DEBUG
		output( Debug, "parser: command: pushing from level %d", context->control.level );
#endif
		// special case: pushing a loop in interactive mode, but no result to loop over
		if ( context_check( 0, 0, ExecutionMode ) && ( context->input.stack == NULL ) &&
		     !strcmp( next_state, loop ) && ( context->expression.results == NULL ))
			; // nothing to push for in this case
		else {
			event = push( *state, event, &base, context );
			*state = base;
			if ( !strcmp( next_state, expression_clause ) ) {
				event = set_expression_clause( *state, event, &next_state, context );
			}
			else if ( !strcmp( next_state, variable_clause ) ) {
				event = set_variable_clause( *state, event, &next_state, context );
			}
			else if ( !strcmp( next_state, loop ) ) {
				event = set_loop( *state, event, &next_state, context );
			}
		}
	}
	else if ( !strcmp( next_state, pop_state ) )
	{
		event = action( *state, event, &next_state, context );
		*state = next_state;
	}
	else if ( !strcmp( next_state, out ) )
	{
		if ( context->narrative.mode.action.one ) {
			action( *state, event, &next_state, context );
			clear_output_mode( *state, event, &next_state, context );
			*state = "";
		} else {
			event = action( *state, event, &next_state, context );
			clear_output_mode( *state, event, &next_state, context );
			*state = base;
		}
	}
	else
	{
		event = action( *state, event, &next_state, context );
		if ( strcmp( next_state, same ) )
			*state = next_state;
	}
	return event;
}

/*---------------------------------------------------------------------------
	clause actions
---------------------------------------------------------------------------*/
static int
set_clause_to_contrary( char *state, int event, char **next_state, _context *context )
{
	if ( !context_check( 0, 0, ExecutionMode ) )
		return 0;

	context->control.contrary = ~context->control.contrary;
	return 0;
}

static int
set_clause( int clause, char *state, int event, char **next_state, _context *context )
{
	switch ( context->control.mode ) {
	case FreezeMode:
		break;
	case InstructionMode:
		; StackVA *stack = (StackVA *) context->control.stack->ptr;
		stack->clause.state = ClauseActive;
		break;
	case ExecutionMode:
		if ( context->expression.mode == ReadMode ) {
			freeLiteral( &context->expression.results );
		}

		stack = (StackVA *) context->control.stack->ptr;
		stack->clause.state = clause ?
			( context->control.contrary ? ClausePassive : ClauseActive ) :
			( context->control.contrary ? ClauseActive : ClausePassive );
		if ( stack->clause.state == ClausePassive )
			set_control_mode( FreezeMode, event, context );
	}
	context->control.contrary = 0;
	return 0;
}

static int
set_variable_clause( char *state, int event, char **next_state, _context *context )
{
	int clause = ( lookupVariable( context, context->identifier.id[ 0 ].ptr ) != NULL );
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
	StackVA *stack = (StackVA *) context->control.stack->ptr;
	switch ( context->control.mode ) {
	case FreezeMode:
		if ( context->control.level == context->freeze.level )
		{
			stack->clause.state = ClauseActive;
			set_control_mode( ExecutionMode, event, context );
		}
		break;
	case InstructionMode:
		switch ( stack->clause.state ) {
			case ClauseActive:
			case ClausePassive:
				break;
			case ClauseNone:
				return output( Error, "not in conditional execution mode" );
		}
		break;
	case ExecutionMode:
		switch ( stack->clause.state ) {
			case ClauseActive:
				stack->clause.state = ClausePassive;
				break;
			case ClausePassive:
				stack->clause.state = ClauseActive;
				break;
			case ClauseNone:
				return output( Error, "not in conditional execution mode" );
		}
		if ( stack->clause.state == ClausePassive )
			set_control_mode( FreezeMode, event, context );
	}
	return 0;
}

/*---------------------------------------------------------------------------
	loop actions
---------------------------------------------------------------------------*/
static int
set_loop( char *state, int event, char **next_state, _context *context )
{
	if ( !context_check( 0, 0, ExecutionMode ) )
		return 0;

	StackVA *stack = (StackVA *) context->control.stack->ptr;
	if ( context->input.stack == NULL ) {
		// here we are reading instructions from stdin
		set_control_mode( InstructionMode, event, context );
	}
	else {
		InputVA *input = (InputVA *) context->input.stack->ptr;
		if ( input->mode == InstructionBlock ) {
			// here we are about to start a loop within a loop
			stack->loop.begin = context->input.instruction->next;
		}
		else {
			// here we may be reading from a script
			set_control_mode( InstructionMode, event, context );
		}
	}
	if ( context->expression.results == NULL ) {
		set_control_mode( FreezeMode, event, context );
	}
	else {
		stack->loop.index = context->expression.results;
		context->expression.results = NULL;
		int type = (( context->expression.mode == ReadMode ) ? LiteralVariable : EntityVariable );
		assign_variator_variable( stack->loop.index->ptr, type, context );
	}
	return 0;
}

/*---------------------------------------------------------------------------
	pop2
---------------------------------------------------------------------------*/
static int
pop2( char *state, int event, char **next_state, _context *context )
{
	StackVA *stack = (StackVA *) context->control.stack->ptr;
	int do_pop = 1;

	switch ( context->control.mode ) {
	case FreezeMode:
		break;
	case InstructionMode:
		if ( context->record.instructions == NULL ) {
			break;	// not recording - most likely replaying
		}
		if ( context->control.level != context->record.level ) {
			if ( !strcmp( state, "/." ) ) {
				return output( Error, "'/.' only allowed to close instruction block..." );
			}
			break;
		}
		// popping from narrative's block mode
		// -----------------------------------
		if ( context->narrative.mode.action.block ) {
			if ( context->record.instructions->next == NULL ) {
				if ( !strcmp( state, "/." ) ) {
					output( Warning, "no action specified - will be removed from narrative..." );
					freeInstructionBlock( context );
				}
				else {
					free ( context->record.instructions->ptr );
					context->record.instructions->ptr = strdup( "~." );
				}
			}
			if ( !strcmp( state, "/" ) ) {
				*next_state = "";
				do_pop = 0;
			}
			break;
		}
		// popping from loop instruction mode - launches execution
		// -------------------------------------------------------
		// check that we do actually have recorded instructions to execute besides '/'
		if ( context->record.instructions->next == NULL ) {
			freeListItem( &stack->loop.index );
			freeInstructionBlock( context );
		} else {
			reorderListItem( &context->record.instructions );
			stack->loop.begin = context->record.instructions;
			push_input( "", context->record.instructions, InstructionBlock, context );

			*next_state = base;
			do_pop = 0;
		}
		set_control_mode( ExecutionMode, event, context );
		break;
	case ExecutionMode:
		// popping during narrative block execution
		// ----------------------------------------
		if (( context->narrative.mode.action.block ) && ( context->control.level == context->narrative.level )) {
			if ( !strcmp( state, "/" ) ) {
				*next_state = "";
				do_pop = 0;
			}
			break;
		}
		// popping during loop execution
		// -----------------------------
		if ( stack->loop.begin == NULL ) {
			;	// not in a loop
		}
		else if ( stack->loop.index->next == NULL ) {
			freeListItem( &stack->loop.index );
			if ( context->control.level == context->record.level ) {
				pop_input( state, event, next_state, context );
				freeInstructionBlock( context );
			}
			else {
				set_instruction( context->input.instruction->next, context );
			}
		}
		else {
			popListItem( &stack->loop.index );
			assign_variator_variable( stack->loop.index->ptr, 0, context );
			set_instruction( stack->loop.begin, context );
			*next_state = base;
			do_pop = 0;
		}
		break;
	}

	if ( do_pop )
	{
		do_( pop, pop_state );

		if ( context->control.mode == FreezeMode ) {
			*next_state = base;
		}
		else if ( context->narrative.mode.action.block && ( context->control.level < context->record.level ) )
		{
			// popping from narrative instruction block - return control to narrative
			*next_state = "";
		}
		else {
			*next_state = state;
		}
	}

#ifdef DEBUG
	output( Debug, "pop2: state=\"%s\"", state );
#endif
	return ( do_pop ? event : 0 );
}

/*---------------------------------------------------------------------------
	command_error
---------------------------------------------------------------------------*/
static int
command_error( char *state, int event, char **next_state, _context *context )
{
	int test = ( context->narrative.mode.action.one || context->narrative.mode.script );
	if ( !strcmp( state, base ) && !test) {
		do_( flush_input, same );
		*next_state = base;
	} else if ( test ) {
		*next_state = "";
	} else {
		do_( error, base );
		*next_state = base;
	}
	return event;
}

/*---------------------------------------------------------------------------
	space
---------------------------------------------------------------------------*/
int
space( char *state, int event, char **next_state, _context *context )
{
	// in general, we want to ignore ' ' and '\t'
	// but there are special cases...

	char *backup = state;

	switch ( state[ strlen(state) - 1 ] ) {
	case '%':
	case 'N':
		do_( error, base )
		break;
	default:
		bgn_
		in_( "!" )			do_( error, base )
		in_( ": identifier : !" )	do_( error, base )
		in_( "> ." )			do_( error, base )
		in_( ">:< \\" )			do_( error, base )
		in_( ">:_" )			do_( output_char, ">:_" )
		in_( ">: \\" )			do_( output_char, ">:_" )
		in_( ">: %variable" )		do_( output_variable_value, ">:_" )
		in_( ">: %[_]" )		do_( output_results, ">:_" )
		in_( ">: %[_].$(_)" )		do_( output_va, ">:_" )
		in_( ">: %[_].narrative(_)" )	do_( output_narrative, ">:_" )
		in_other
		if ( context->narrative.mode.action.one ) {
			bgn_
			in_( "!. %[_].narrative(_)" )	do_( narrative_op, out )
			in_( "!. narrative(_)" )	do_( narrative_op, out )
			in_( ">@session:_" )		do_( session_cmd, out )
			in_( ">@session:_ >:" )		do_( session_cmd, out )
			in_( ">:< \\Nfile://path" )	do_( input_story, out )
			in_( ":~ identifier" )		do_( variable_reset, out )
			in_( ":< %( string )" )		do_( input_command, out )
			in_other			do_( nop, same )
			end
		} else {
			do_( nop, same )
		}
		end
	}
	if ( strcmp( state, backup ) ) *next_state = state;
	return event;
}

/*---------------------------------------------------------------------------
	read_command
---------------------------------------------------------------------------*/
#define BRKOUT if ( context->narrative.mode.action.one ) do_( nothing, "" ) else
#define BRKERR if ( context->narrative.mode.action.one ) do_( error, "" ) else

int
read_command( char *state, int event, char **next_state, _context *context )
{
	do {
	event = io_scan( state, event, NULL, context );

#ifdef DEBUG
	output( Debug, "main: state=\"%s\", event='%c'", state, event );
#endif
	bgn_
	on_( 0 )	do_( nothing, "" ) // input() popped from ClientInput mode
	on_( -1 )	do_( command_error, base )
	on_( ' ' )	do_( space, same )
	on_( '\t' )	do_( space, same )

	in_( base ) bgn_
		on_( '\n' )	do_( nop, same )
		on_( ':' )	do_( nop, ":" )
		on_( '>' )	do_( nop, ">" )
		on_( '%' )	do_( nop, "%" )		//---- HCN MODE ONLY
		on_( '?' )	BRKERR do_( nop, "?" )
		on_( '/' )	BRKERR do_( nop, "/" )
		on_( '!' )	do_( nop, "!" )
		on_( '~' )	do_( nop, "~" )
		on_( 'e' )	do_( on_token, "exit\0exit" )
		on_other	do_( error, base )
		end

	in_( "exit" ) bgn_
		on_( '\n' )	do_( exit_narrative, out ) // ONLY IN NARRATIVE MODE
		on_other	do_( error, base )
		end

	in_( "/" ) bgn_
		on_( '~' )	do_( nop, "/~" )
		on_( '\n' )	do_( pop2, pop_state )
		on_( '.' )	do_( nop, "/." )
		on_other	do_( error, base )
		end
		in_( "/~" ) bgn_
			on_( '\n' )	do_( flip_clause, base )
			on_other	do_( error, base )
			end
		in_( "/." ) bgn_
			on_( '\n' )	do_( pop2, pop_state )
			on_other	do_( error, base )
			end

	in_( "!" ) bgn_
		on_( '!' )	do_( set_expression_mode, "!." )
		on_( '~' )	do_( set_expression_mode, "!." )
		on_( '*' )	do_( set_expression_mode, "!." )
		on_( '_' )	do_( set_expression_mode, "!." )
		on_other	do_( error, base )
		end
		in_( "!." ) bgn_
			on_( '%' )	do_( nop, "!. %" )
			on_other	do_( parse_expression, "!. expression" )
			end
			in_( "!. %" ) bgn_
				on_( '[' )	do_( nop, "!. %[" )
				on_other	do_( parse_expression, "!. expression" )
				end
				in_( "!. %[" ) bgn_
					on_any	do_( evaluate_expression, "!. %[_" )
					end
				in_( "!. %[_" ) bgn_
					on_( ']' )	do_( nop, "!. %[_]" )
					on_other	do_( error, base )
					end
				in_( "!. %[_]" ) bgn_
					on_( '.' )	do_( nop, "!. %[_]." )
					on_other	do_( error, base )
					end
				in_( "!. %[_]." ) bgn_
					on_any	do_( read_1, "!. %[_].narrative" )
					end
				in_( "!. %[_].narrative" ) bgn_
					on_( '(' )	do_( nop, "!. %[_].narrative(" )
					on_other	do_( error, base )
					end
				in_( "!. %[_].narrative(" ) bgn_
					on_( ')' )	do_( nop, "!. %[_].narrative(_)" )
					on_other	do_( error, base )
					end
			in_( "!. %[_].narrative(_)" ) bgn_
				on_( '\n' )	do_( narrative_op, out )
				on_other	do_( command_error, out )
				end
			in_( "!. narrative(_)" ) bgn_
				on_( '\n' )	do_( narrative_op, out )
				on_other	do_( command_error, out )
				end
			in_( "!. expression" ) bgn_
				on_( '\n' )	do_( expression_op, out )
				on_( '(' )	do_( set_results_to_nil, "!. narrative(" )
				on_other	do_( command_error, out )
				end
				in_( "!. narrative(" ) bgn_
					on_( ')' )	do_( nop, "!. narrative(_)" )
					on_other	do_( error, base )
					end
	in_( "~" ) bgn_
		on_( '.' )	do_( nop, "~." )
		on_other	do_( error, base )
		end
		in_( "~." ) bgn_
			on_( '\n' )	BRKOUT do_( nop, base )
			on_other	BRKOUT do_( error, base )
			end

	//---- HCN mode only
	in_( "%" ) bgn_
		on_( '?' )	BRKERR do_( output_variator_value, base )
		on_other	BRKERR do_( read_0, "%variable" )
		end
		in_( "%variable" ) bgn_
			on_any	do_( output_variable_value, base )
			end
	//---- /

	in_( ">" ) bgn_
		on_( '@' )	do_( nop, ">@" )
		on_( ':' )	do_( set_output_mode, ">:" )
		on_( '.' )	do_( nop, "> ." )
		on_other	do_( read_0, "> identifier" )
		end
		in_( ">@" ) bgn_
			on_any	do_( read_path, ">@session" )
			end
			in_( ">@session" ) bgn_
				on_( ':' )	do_( nop, ">@session:" )
				on_other	do_( error, base )
				end
			in_( ">@session:" ) bgn_
				on_any	do_( read_2, ">@session:_" )
				end
			in_( ">@session:_" ) bgn_
				on_( '>' )	do_( nop, ">@session:_ >" )
				on_( '\n' )	do_( session_cmd, out )
				on_other	do_( command_error, out )
				end
			in_( ">@session:_ >" ) bgn_
				on_( ':' )	do_( nop, ">@session:_ >:" )
				on_other	do_( error, base )
				end
			in_( ">@session:_ >:" ) bgn_
				on_( '\n' )	do_( session_cmd, out )
				on_other	do_( command_error, out )
				end
		in_( "> ." ) bgn_
			on_( '.' )	do_( nop, "> .." )
			on_other	do_( error, base )
			end
			in_( "> .." ) bgn_
				on_( ':' )	do_( set_output_mode, ">:" )
				on_other	do_( error, base )
				end
		in_( "> identifier" ) bgn_
			on_( ':' )	do_( set_output_mode, ": identifier :" )
			on_other	do_( error, base )
			end

	in_( ">:" ) bgn_
		on_( '<' )	do_( nop, ">:<" )
		on_( '\\' )	do_( nop, ">: \\" )
		on_( '\n' )	do_( output_char, out )
		on_( '%' )	do_( nop, ">: %" )
		on_other	do_( output_char, ">:_" )
		end
		in_( ">:_" ) bgn_
			on_( '\\' )	do_( nop, ">: \\" )
			on_( '\n' )	do_( output_char, out )
			on_( '%' )	do_( nop, ">: %" )
			on_other	do_( output_char, ">:_" )
			end
		in_( ">: \\" ) bgn_
			on_( '\n' )	do_( output_char, out )
			on_other	do_( output_special_char, ">:_" )
			end
		in_( ">:<" ) bgn_
			on_( '\\' )	do_( nop, ">:< \\" )
			on_other	do_( error, base )
			end
			in_( ">:< \\" ) bgn_
				on_( 'N' )	do_( nop, ">:< \\N" )
				on_other	do_( error, base )
				end
			in_( ">:< \\N" ) bgn_
				on_( 'f' )	do_( on_token, "file:\0>:< \\Nfile:" )
				on_other	do_( error, base )
				end
			in_( ">:< \\Nfile:" ) bgn_
				on_any	do_( read_path, ">:< \\Nfile://path" )
				end
			in_( ">:< \\Nfile://path" ) bgn_
				on_( '\n' )	do_( input_story, out )
				on_other	do_( command_error, out )
				end
		in_( ">: %" ) bgn_
			on_( '?' )	do_( output_variator_value, ">:_" )
			on_( '.' )	do_( set_results_to_nil, ">: %[_]." )
			on_( '[' )	do_( nop, ">: %[" )
			on_( '\n' )	do_( output_mod, out )
			on_separator	do_( output_mod, ">:_" )
			on_other	do_( read_0, ">: %variable" )
			end
			in_( ">: %[" ) bgn_
				on_any	do_( evaluate_expression, ">: %[_" )
				end
			in_( ">: %[_" ) bgn_
				on_( ']' )	do_( nop, ">: %[_]")
				on_other	do_( error, base )
				end
		in_( ">: %variable" ) bgn_
			on_( '\n' )	do_( output_variable_value, out )
			on_other	do_( output_variable_value, ">:_" )
			end
		in_( ">: %[_]" ) bgn_
			on_( '.' )	do_( nop, ">: %[_]." )
			on_( '\n' )	do_( output_results, out )
			on_other	do_( output_results, ">:_" )
			end
			in_( ">: %[_]." ) bgn_
				on_( '$' )	do_( read_va, ">: %[_].$(_)" )
				on_other	do_( read_1, ">: %[_].narrative" )
				end
			in_( ">: %[_].narrative" ) bgn_
				on_( '(' )	do_( nop, ">: %[_].narrative(" )
				on_other	do_( error, base )
				end
				in_( ">: %[_].narrative(" ) bgn_
					on_( ')' )	do_( nop, ">: %[_].narrative(_)" )
					on_other	do_( error, base )
					end
		in_( ">: %[_].$(_)" ) bgn_
			on_( '\n' )	do_( output_va, out )
			on_other	do_( output_va, ">:_" )
			end
		in_( ">: %[_].narrative(_)" ) bgn_
			on_( '\n' )	do_( output_narrative, out )
			on_other	do_( output_narrative, ">:_" )
			end

	in_( "?" ) bgn_
		on_( '%' )	do_( nop, "? %" )
		on_( '~' )	do_( nop, "?~" )
		on_( '\n' )	do_( error, base )
		on_( ':' )	do_( nop, "? identifier:" )
		on_other	do_( read_0, "? identifier" )
		end
		in_( "? %" ) bgn_
			on_( ':' )	do_( nop, "? %:" )
			on_( '[' )	do_( nop, "? %[" )
			on_other	do_( read_0, "? %variable" )
			end
			in_( "? %:" ) bgn_
				on_any	do_( evaluate_expression, "? %: expression" )
				end
				in_( "? %: expression" ) bgn_
					on_( '\n' )	do_( push, expression_clause )
					on_other	do_( error, base )
					end
			in_( "? %[" ) bgn_
				on_any	do_( evaluate_expression, "? %[_" )
				end
				in_( "? %[_" ) bgn_
					on_( ']' )	do_( nop, "? %[_]" )
					on_other	do_( error, base )
					end
					in_( "? %[_]" ) bgn_
						on_( '\n' )	do_( push, expression_clause )
						on_other	do_( error, base )
						end
			in_( "? %variable" ) bgn_
				on_( '\n' )	do_( push, variable_clause )
				on_other	do_( error, base )
				end

		in_( "?~" ) bgn_
			on_( '%' )	do_( nop, "?~ %" )
			on_other	do_( error, base )
			end
			in_( "?~ %" ) bgn_
				on_( ':' )	do_( nop, "?~ %:" )
				on_( '[' )	do_( nop, "?~ %[" )
				on_other	do_( read_0, "?~ %variable" )
				end
				in_( "?~ %:" ) bgn_
					on_any	do_( evaluate_expression, "?~ %: expression" )
					end
					in_( "?~ %: expression" ) bgn_
						on_( '\n' )	do_( set_clause_to_contrary, same )
								do_( push, expression_clause )
						on_other	do_( error, base )
						end
	
				in_( "?~ %[" ) bgn_
					on_any	do_( evaluate_expression, "?~ %[_" )
					end
					in_( "?~ %[_" ) bgn_
						on_( ']' )	do_( nop, "?~ %[_]" )
						on_other	do_( error, base )
						end
						in_( "?~ %[_]" ) bgn_
							on_( '\n' )	do_( set_clause_to_contrary, same )
									do_( push, expression_clause )
							on_other	do_( error, base )
									end
				in_( "?~ %variable" ) bgn_
					on_( '\n' )	do_( set_clause_to_contrary, same )
							do_( push, variable_clause )
					on_other	do_( error, base )
					end

		in_( "? identifier" ) bgn_
			on_( '\n' )	do_( error, base )
			on_( ':' )	do_( nop, "? identifier:" )
			on_other	do_( error, base )
			end
			in_( "? identifier:" ) bgn_
				on_( '%' )	do_( nop, "? identifier: %" )
				on_( '\n' )	do_( error, base )
				on_other	do_( evaluate_expression, "? identifier: expression" )
				end
				in_( "? identifier: expression" ) bgn_
					on_( '\n' )	do_( push, loop )
					on_other	do_( error, base )
					end
				in_( "? identifier: %" ) bgn_
					on_( '[' )	do_( nop, "? identifier: %[" )
					on_other	do_( error, base )
					end
					in_( "? identifier: %[" ) bgn_
						on_any	do_( evaluate_expression, "? identifier: %[_" )
						end
					in_( "? identifier: %[_" ) bgn_
						on_( ']' )	do_( nop, "? identifier: %[_]" )
						on_other	do_( error, base )
						end
					in_( "? identifier: %[_]" ) bgn_
						on_( '\n' )	do_( push, loop )
						on_other	do_( error, base )
						end
	in_( ":" ) bgn_
		on_( '\n' )	do_( error, base )
		on_( '%' )	do_( nop, ": %" )
		on_( '<' )	do_( nop, ":<" )
		on_( '~' )	do_( nop, ":~" )
		on_other	do_( read_0, ": identifier" )
		end
		in_( ": identifier" ) bgn_
			on_( '\n' )	do_( error, base )
			on_( ':' )	do_( set_output_mode, ": identifier :" )
			on_other	do_( error, base )
			end
			in_( ": identifier :" ) bgn_
				on_( '%' )	do_( nop, ": identifier : %" )
				on_( '\n' )	do_( error, base )
				on_( '!' )	do_( nop, ": identifier : !" )
				on_other	do_( parse_expression, ": identifier : expression" )
				end
				in_( ": identifier : expression" ) bgn_
					on_( '(' )	do_( set_results_to_nil, ": identifier : narrative(" )
					on_( '\n' )	do_( assign_expression, out )
					on_other	do_( error, base )
					end
				in_( ": identifier : %" ) bgn_
					on_( '[' )	do_( nop, ": identifier : %[" )
					on_other	do_( parse_expression, ": identifier : expression" )
					end
					in_( ": identifier : %[" ) bgn_
						on_any	do_( evaluate_expression, ": identifier : %[_" )
						end
						in_( ": identifier : %[_" ) bgn_
							on_( ']' )	do_( nop, ": identifier : %[_]" )
							on_other	do_( error, base )
							end
						in_( ": identifier : %[_]" ) bgn_
							on_( '\n' )	do_( assign_results, out )
							on_( '.' )	do_( nop, ": identifier : %[_]." )
							on_other	do_( error, base )
							end
						in_( ": identifier : %[_]." ) bgn_
							on_( '$' )	do_( read_va, ": identifier : %[_].$(_)" )
							on_other	do_( read_1, ": identifier : %[_].narrative" )
							end
						in_( ": identifier : %[_].$(_)" ) bgn_
							on_( '\n' )	do_( assign_va, out )
							on_other	do_( error, base )
							end
						in_( ": identifier : %[_].narrative" ) bgn_
							on_( '(' )	do_( nop, ": identifier : narrative(" )
							on_other	do_( error, base )
							end
				in_( ": identifier : !" ) bgn_
					on_( '!' )	do_( set_expression_mode, same )
							do_( parse_expression, ": identifier : !." )
					on_( '~' )	do_( set_expression_mode, same )
							do_( parse_expression, ": identifier : !." )
					on_( '*' )	do_( set_expression_mode, same )
							do_( parse_expression, ": identifier : !." )
					on_( '_' )	do_( set_expression_mode, same )
							do_( parse_expression, ": identifier : !." )
					on_other	do_( error, base )
					end
				in_( ": identifier : !." ) bgn_
					on_any	do_( expression_op, ": identifier : !. expression" )
					end
					in_( ": identifier : !. expression" ) bgn_
						on_( '(' )	do_( set_results_to_nil, ": identifier : !. narrative(" )
						on_( '\n' )	do_( assign_results, out )
						on_other	do_( error, base )
						end
					in_( ": identifier : !. narrative(" ) bgn_
						on_( ')' )	do_( nop, ": identifier : !. narrative(_)" )
						on_other	do_( error, base )
						end
					in_( ": identifier : !. narrative(_)" ) bgn_
						on_( '\n' )	do_( narrative_op, same )
								do_( assign_narrative, out )
						on_other	do_( error, base )
						end
				in_( ": identifier : narrative(" ) bgn_
					on_( ')' )	do_( nop, ": identifier : narrative(_)" )
					on_other	do_( error, base )
					end
					in_( ": identifier : narrative(_)" ) bgn_
						on_( '\n' )	do_( assign_narrative, out )
						on_other	do_( error, base )
						end

		in_( ": %" ) bgn_
			on_( '.' )	do_( set_results_to_nil, ": %[_]." )
			on_( '[' )	do_( nop, ": %[" )
			on_other	do_( error, base )
			end
			in_( ": %[" ) bgn_
				on_any	do_( evaluate_expression, ": %[_" )
				end
				in_( ": %[_" ) bgn_
					on_( ']' )	do_( nop, ": %[_]" )
					on_other	do_( error, base )
					end
				in_( ": %[_]" ) bgn_
					on_( '.' )	do_( nop, ": %[_]." )
					on_other	do_( error, base )
					end
			in_( ": %[_]." ) bgn_
				on_( '$' )	do_( read_va, ": %[_].$(_)" )
				on_other	do_( error, base )
				end
				in_( ": %[_].$(_)" ) bgn_
					on_( ':' )	do_( nop, ": %[_].$(_):" )
					on_other	do_( error, base )
					end
				in_( ": %[_].$(_):" ) bgn_
					on_( '%' )	do_( nop, ": %[_].$(_): %" )
					on_other	do_( read_1, ": %[_].$(_): arg" )
					end
					in_( ": %[_].$(_): %" ) bgn_
						on_any	do_( read_1, ": %[_].$(_): %arg" )
						end
						in_( ": %[_].$(_): %arg" ) bgn_
							on_( '\n' )	do_( set_va_from_variable, out )
							on_other	do_( error, base )
							end
					in_( ": %[_].$(_): arg" ) bgn_
						on_( '(' )	do_( nop, ": %[_].$(_): arg(" )
						on_( '\n' )	do_( set_va, out )
						on_other	do_( error, base )
						end
					in_( ": %[_].$(_): arg(" ) bgn_
						on_( ')' )	do_( nop, ": %[_].$(_): arg(_)" )
						on_other	do_( error, base )
						end
					in_( ": %[_].$(_): arg(_)" ) bgn_
						on_( '\n' )	do_( set_va, out )
						on_other	do_( error, base )
						end

		in_( ":~" ) bgn_
			on_( '\n' )	do_( reset_variables, out )
			on_other	do_( read_0, ":~ identifier" )
			end
			in_( ":~ identifier" ) bgn_
				on_( '\n' )	do_( variable_reset, out )
				on_other	do_( command_error, out )
				end
		in_( ":<" ) bgn_
			on_( '%' )	do_( nop, ":< %" )
			on_other	do_( error, base )
			end
			in_( ":< %" ) bgn_
				on_( '(' )	do_( nop, ":< %(" )
				on_other	do_( error, base )
				end
			in_( ":< %(" ) bgn_
				on_other	do_( read_0, ":< %( string" )
				end
			in_( ":< %( string" ) bgn_
				on_( ')' )	do_( nop, ":< %( string )" )
				on_other	do_( error, base )
				end
			in_( ":< %( string )" ) bgn_
				on_( '\n' )	do_( input_command, out )
				on_other	do_( command_error, out )
				end
	end

	}
	while ( strcmp( state, "" ) );

	return event;
}

