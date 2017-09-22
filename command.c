#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "database.h"
#include "registry.h"
#include "kernel.h"

#include "input.h"
#include "output.h"
#include "io.h"
#include "expression.h"
#include "variables.h"
#include "value.h"
#include "string_util.h"
#include "command.h"
#include "command_util.h"

// #define DEBUG

/*---------------------------------------------------------------------------
	execution engine
---------------------------------------------------------------------------*/
#define	command_do_( a, s ) \
	event = command_execute( a, &state, event, s, context );

static int
command_execute( _action action, char **state, int event, char *next_state, _context *context )
{
	int retval;

	if ( action == push )
	{
#ifdef DEBUG
		output( Debug, "parser: command: pushing from level %d", context->control.level );
#endif
		event = push( *state, event, &next_state, context );
		*state = base;
	}
	else if ( !strcmp( next_state, pop_state ) )
	{
		event = action( *state, event, &next_state, context );
		*state = next_state;
	}
	else if ( !strcmp( next_state, out ) )
	{
		action( *state, event, &next_state, context );
		*state = "";
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
	condition actions
---------------------------------------------------------------------------*/
static int
set_condition_to_contrary( char *state, int event, char **next_state, _context *context )
{
	if ( !context_check( 0, 0, ExecutionMode ) )
		return 0;

	context->control.contrary = ~context->control.contrary;
	return 0;
}

static int
push_condition( int condition, char *state, int event, char **next_state, _context *context )
{
	command_do_( push, *next_state );
	*next_state = base;

	switch ( context->control.mode ) {
	case FreezeMode:
		break;
	case InstructionMode:
		; StackVA *stack = (StackVA *) context->control.stack->ptr;
		stack->condition = ConditionActive;
		break;
	case ExecutionMode:
		if ( context->expression.mode == ReadMode ) {
			freeLiteral( &context->expression.results );
		}

		stack = (StackVA *) context->control.stack->ptr;
		stack->condition = condition ?
			( context->control.contrary ? ConditionPassive : ConditionActive ) :
			( context->control.contrary ? ConditionActive : ConditionPassive );
		if ( stack->condition == ConditionPassive )
			set_control_mode( FreezeMode, event, context );
	}
	context->control.contrary = 0;
	return 0;
}

static int
push_condition_from_variable( char *state, int event, char **next_state, _context *context )
{
	int condition = ( lookupVariable( context, context->identifier.id[ 0 ].ptr ) != NULL );
	return push_condition( condition, state, event, next_state, context );
}

static int
push_condition_from_expression( char *state, int event, char **next_state, _context *context )
{
	int condition = ( context->expression.results != NULL );
	return push_condition( condition, state, event, next_state, context );
}

static int
flip_condition( char *state, int event, char **next_state, _context *context )
{
	StackVA *stack = (StackVA *) context->control.stack->ptr;
	switch ( context->control.mode ) {
	case FreezeMode:
		if ( context->control.level == context->freeze.level )
		{
			stack->condition = ConditionActive;
			set_control_mode( ExecutionMode, event, context );
		}
		break;
	case InstructionMode:
		switch ( stack->condition ) {
			case ConditionActive:
			case ConditionPassive:
				break;
			case ConditionNone:
				return output( Error, "not in conditional execution mode" );
		}
		break;
	case ExecutionMode:
		switch ( stack->condition ) {
			case ConditionActive:
				stack->condition = ConditionPassive;
				break;
			case ConditionPassive:
				stack->condition = ConditionActive;
				break;
			case ConditionNone:
				return output( Error, "not in conditional execution mode" );
		}
		if ( stack->condition == ConditionPassive )
			set_control_mode( FreezeMode, event, context );
	}
	return 0;
}

/*---------------------------------------------------------------------------
	loop actions
---------------------------------------------------------------------------*/
static int
push_loop( char *state, int event, char **next_state, _context *context )
{
	if (( context->control.mode == ExecutionMode ) &&
	    ( context->input.stack == NULL ) && ( context->expression.results == NULL ))
	{
		return 0;	// do nothing in this case
	}

	command_do_( push, *next_state );
	*next_state = base;

	if ( !context_check( 0, 0, ExecutionMode ) )
		return 0;

	StackVA *stack = (StackVA *) context->control.stack->ptr;
	if ( context->input.stack == NULL ) {
		// here we are reading instructions from stdin
		set_control_mode( InstructionMode, event, context );
	}
	else {
		StreamVA *input = (StreamVA *) context->input.stack->ptr;
		if ( input->type == InstructionBlock ) {
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
	command_pop
---------------------------------------------------------------------------*/
static int
command_pop( char *state, int event, char **next_state, _context *context )
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
			push_input( NULL, context->record.instructions, InstructionBlock, context );

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
		command_do_( pop, pop_state );

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
	output( Debug, "command_pop: to state=\"%s\"", state );
#endif
	return ( do_pop ? event : 0 );
}

/*---------------------------------------------------------------------------
	command_error
---------------------------------------------------------------------------*/
static int
command_error( char *state, int event, char **next_state, _context *context )
{
	if ( context->narrative.mode.action.one || context->narrative.mode.script ) {
		*next_state = "";
	} else {
		command_do_( flush_input, same );
		*next_state = base;
	}
	return event;
}

/*---------------------------------------------------------------------------
	read_command
---------------------------------------------------------------------------*/
#define BRKOUT if ( context->narrative.mode.action.one ) command_do_( nothing, "" ) else
#define BRKERR if ( context->narrative.mode.action.one ) command_do_( error, "" ) else
#define RETURN ( context->narrative.mode.action.one ? out : base )

int
read_command( char *state, int event, char **next_state, _context *context )
{
	do {
	event = io_read( state, event, NULL, context );

#ifdef DEBUG
	output( Debug, "main: \"%s\", '%c'", state, event );
#endif

	bgn_
	on_( 0 )	command_do_( nothing, "" )
	on_( -1 )	command_do_( command_error, base )

	in_( base ) bgn_
		on_( ' ' )	command_do_( nop, same )
		on_( '\t' )	command_do_( nop, same )
		on_( '\n' )	command_do_( nop, same )
		on_( ':' )	command_do_( nop, ":" )
		on_( '>' )	command_do_( nop, ">" )
		on_( '%' )	command_do_( nop, "%" )		// HCN MODE ONLY
		on_( '?' )	BRKERR command_do_( nop, "?" )
		on_( '/' )	BRKERR command_do_( nop, "/" )
		on_( '!' )	command_do_( nop, "!" )
		on_( '~' )	command_do_( nop, "~" )
		on_( 'e' )	command_do_( nop, "e" )
		on_other	command_do_( error, base )
		end

	in_( "!" ) bgn_
		on_( '!' )	command_do_( set_expression_mode, "!." )
		on_( '~' )	command_do_( set_expression_mode, "!." )
		on_( '*' )	command_do_( set_expression_mode, "!." )
		on_( '_' )	command_do_( set_expression_mode, "!." )
		on_other	command_do_( error, base )
		end
		in_( "!." ) bgn_
			on_( ' ' )	command_do_( nop, same )
			on_( '\t' )	command_do_( nop, same )
			on_( '%' )	command_do_( nop, "!. %" )
			on_other	command_do_( read_expression, "!. expression" )
			end
			in_( "!. expression" )
				if ( context->narrative.mode.action.one ) bgn_
					on_( ' ' )	command_do_( expression_op, out )
					on_( '\t' )	command_do_( expression_op, out )
					on_( '\n' )	command_do_( expression_op, out )
					on_( '(' )	command_do_( set_results_to_nil, "!. narrative(" )
					on_other	command_do_( nothing, out )
					end
				else bgn_
					on_( ' ' )	command_do_( nop, same )
					on_( '\t' )	command_do_( nop, same )
					on_( '\n' )	command_do_( expression_op, base )
					on_( '(' )	command_do_( set_results_to_nil, "!. narrative(" )
					on_other	command_do_( error, base )
					end
			in_( "!. narrative(" ) bgn_
				on_( ' ' )	command_do_( nop, same )
				on_( '\t' )	command_do_( nop, same )
				on_( ')' )	command_do_( nop, "!. narrative(_)" )
				on_other	command_do_( error, base )
				end
				in_( "!. narrative(_)" )
					if ( context->narrative.mode.action.one ) bgn_
						on_( ' ' )	command_do_( narrative_op, out )
						on_( '\t' )	command_do_( narrative_op, out )
						on_( '\n' )	command_do_( narrative_op, out )
						on_other	command_do_( nothing, out )
						end
					else bgn_
						on_( ' ' )	command_do_( nop, same )
						on_( '\t' )	command_do_( nop, same )
						on_( '\n' )	command_do_( narrative_op, base )
						on_other	command_do_( error, base )
						end
			in_( "!. %" ) bgn_
				on_( '[' )	command_do_( nop, "!. %[" )
				on_other	command_do_( read_expression, "!. expression" )
				end
				in_( "!. %[" ) bgn_
					on_any	command_do_( evaluate_expression, "!. %[_" )
					end
					in_( "!. %[_" ) bgn_
						on_( ']' )	command_do_( nop, "!. %[_]" )
						on_other	command_do_( error, base )
						end
						in_( "!. %[_]" ) bgn_
							on_( '.' )	command_do_( nop, "!. %[_]." )
							on_other	command_do_( error, base )
							end
							in_( "!. %[_]." ) bgn_
								on_any	command_do_( read_1, "!. %[_].arg" )
								end
								in_( "!. %[_].arg" ) bgn_
									on_( ' ' )	command_do_( nop, same )
									on_( '\t' )	command_do_( nop, same )
									on_( '(' )	command_do_( nop, "!. %[_].arg(" )
									on_other	command_do_( error, base )
									end
									in_( "!. %[_].arg(" ) bgn_
										on_( ' ' )	command_do_( nop, same )
										on_( '\t' )	command_do_( nop, same )
										on_( ')' )	command_do_( nop, "!. %[_].arg(_)" )
										on_other	command_do_( error, base )
										end
										in_( "!. %[_].arg(_)" )
											if ( context->narrative.mode.action.one ) bgn_
												on_( ' ' )	command_do_( narrative_op, out )
												on_( '\t' )	command_do_( narrative_op, out )
												on_( '\n' )	command_do_( narrative_op, out )
												on_other	command_do_( nothing, out )
												end
											else bgn_
												on_( ' ' )	command_do_( nop, same )
												on_( '\t' )	command_do_( nop, same )
												on_( '\n' )	command_do_( narrative_op, base )
												on_other	command_do_( error, base )
												end
	in_( "~" ) bgn_
		on_( '.' )	command_do_( nop, "~." )
		on_other	command_do_( error, base )
		end
		in_( "~." ) bgn_
			on_( '\n' )	BRKOUT command_do_( nop, base )
			on_other	command_do_( error, base )
			end
	in_( "e" ) bgn_
		on_( 'x' )	command_do_( nop, "ex" )
		on_other	command_do_( error, base )
		end
		in_( "ex" ) bgn_
			on_( 'i' )	command_do_( nop, "exi" )
			on_other	command_do_( error, base )
			end
			in_( "exi" ) bgn_
				on_( 't' )	command_do_( nop, "exit" )
				on_other	command_do_( error, base )
				end
				in_( "exit" ) bgn_
					on_( '\n' )	command_do_( exit_narrative, RETURN ) // ONLY IN NARRATIVE MODE
					on_other	command_do_( error, base )
					end

	in_( "%" ) bgn_
		on_( '?' )	BRKERR command_do_( output_variator_value, base )	// ONLY IN HCN MODE
		on_other	BRKERR command_do_( read_0, "%variable" )	// ONLY IN HCN MODE
		end
		in_( "%variable" ) bgn_
			on_any	command_do_( output_variable_value, base )
			end

	in_( "/" ) bgn_
		on_( '~' )	command_do_( nop, "/~" )
		on_( '\n' )	command_do_( command_pop, pop_state )
		on_( '.' )	command_do_( nop, "/." )
		on_other	command_do_( error, base )
		end
		in_( "/~" ) bgn_
			on_( '\n' )	command_do_( flip_condition, base )
			on_other	command_do_( error, base )
			end
		in_( "/." ) bgn_
			on_( '\n' )	command_do_( command_pop, pop_state )
			on_other	command_do_( error, base )
			end

	in_( ">" ) bgn_
		on_( ' ' )	command_do_( nop, same )
		on_( '\t' )	command_do_( nop, same )
		on_( ':' )	command_do_( nop, ">:" )
		on_other	command_do_( read_0, ">_" )
		end
		in_( ">_" ) bgn_
			on_( ' ' )	command_do_( nop, same )
			on_( '\t' )	command_do_( nop, same )
			on_( ':' )	command_do_( set_assignment_mode, ": identifier :" )
			on_other	command_do_( error, base )
			end
		in_( ">:" ) bgn_
			on_( '\n' )	command_do_( output_char, RETURN )
			on_( '%' )	command_do_( nop, ">: %" )
			on_other	command_do_( output_char, same )
			end
			in_( ">: %" ) bgn_
				on_( '?' )	command_do_( output_variator_value, ">:" )
				on_( '.' )	command_do_( set_results_to_nil, ">: %[_]." )
				on_( '[' )	command_do_( nop, ">: %[" )
				on_( '\n' )	command_do_( output_mod, RETURN )
				on_separator	command_do_( output_mod, ">:" )
				on_other	command_do_( read_0, ">: %variable" )
				end
				in_( ">: %variable" ) bgn_
					on_( '\n' )	command_do_( output_variable_value, RETURN )
					on_other	command_do_( output_variable_value, ">:" )
					end
				in_( ">: %[" ) bgn_
					on_any	command_do_( evaluate_expression, ">: %[_" )
					end
					in_( ">: %[_" ) bgn_
						on_( ' ' )	command_do_( nop, same )
						on_( '\t' )	command_do_( nop, same )
						on_( ']' )	command_do_( nop, ">: %[_]")
						on_other	command_do_( error, base )
						end
						in_( ">: %[_]" ) bgn_
							on_( '.' )	command_do_( nop, ">: %[_]." )
							on_( '\n' )	command_do_( output_results, RETURN )
							on_other	command_do_( output_results, ">:" )
							end
					in_( ">: %[_]." ) bgn_
						on_( '$' )	command_do_( read_va, ">: %[_].$" )
						on_( '%' )	command_do_( nop, ">: %[_].%" )
						on_other	command_do_( read_1, ">: %[_].arg" )
						end
						in_( ">: %[_].$" ) bgn_
							on_( '\n' )	command_do_( output_va, RETURN )
							on_other	command_do_( output_va, ">:" )
							end
						in_( ">: %[_].arg" ) bgn_
							on_( ' ' )	command_do_( nop, same )
							on_( '\t' )	command_do_( nop, same )
							on_( '(' )	command_do_( nop, ">: %[_].narrative(" )
							on_other	command_do_( error, base )
							end
							in_( ">: %[_].narrative(" ) bgn_
								on_( ' ' )	command_do_( nop, same )
								on_( '\t' )	command_do_( nop, same )
								on_( ')' )	command_do_( nop, ">: %[_].narrative(_)" )
								on_other	command_do_( error, base )
								end
								in_( ">: %[_].narrative(_)" ) bgn_
									on_( '\n' )	command_do_( output_narrative, RETURN )
									on_other	command_do_( output_narrative, ">:" )
									end

	in_( "?" ) bgn_
		on_( ' ' )	command_do_( nop, same )
		on_( '\t' )	command_do_( nop, same )
		on_( '%' )	command_do_( nop, "? %" )
		on_( '~' )	command_do_( nop, "?~" )
		on_( '\n' )	command_do_( error, base )
		on_( ':' )	command_do_( nop, "? identifier:" )
		on_other	command_do_( read_0, "? identifier" )
		end

		in_( "? %" ) bgn_
			on_( ':' )	command_do_( nop, "? %:" )
			on_( '[' )	command_do_( nop, "? %[" )
			on_other	command_do_( read_0, "? %variable" )
			end
			in_( "? %:" ) bgn_
				on_( ' ' )	command_do_( nop, same )
				on_( '\t' )	command_do_( nop, same )
				on_other	command_do_( evaluate_expression, "? %: expression" )
				end
				in_( "? %: expression" ) bgn_
					on_( ' ' )	command_do_( nop, same )
					on_( '\t' )	command_do_( nop, same )
					on_( '\n' )	command_do_( push_condition_from_expression, base )
					on_other	command_do_( error, base )
					end
			in_( "? %[" ) bgn_
				on_any	command_do_( evaluate_expression, "? %[_" )
				end
				in_( "? %[_" ) bgn_
					on_( ']' )	command_do_( nop, "? %[_]" )
					on_other	command_do_( error, base )
					end
					in_( "? %[_]" ) bgn_
						on_( ' ' )	command_do_( nop, same )
						on_( '\t' )	command_do_( nop, same )
						on_( '\n' )	command_do_( push_condition_from_expression, base )
						on_other	command_do_( error, base )
						end
			in_( "? %variable" ) bgn_
				on_( ' ' )	command_do_( nop, same )
				on_( '\t' )	command_do_( nop, same )
				on_( '\n' )	command_do_( push_condition_from_variable, base )
				on_other	command_do_( error, base )
				end

		in_( "?~" ) bgn_
			on_( ' ' )	command_do_( nop, same )
			on_( '\t' )	command_do_( nop, same )
			on_( '%' )	command_do_( nop, "?~ %" )
			on_other	command_do_( error, base )
			end
	
			in_( "?~ %" ) bgn_
				on_( ':' )	command_do_( nop, "?~ %:" )
				on_( '[' )	command_do_( nop, "?~ %[" )
				on_other	command_do_( read_0, "?~ %variable" )
				end
				in_( "?~ %:" ) bgn_
					on_( ' ' )	command_do_( nop, same )
					on_( '\t' )	command_do_( nop, same )
					on_other	command_do_( evaluate_expression, "?~ %: expression" )
					end
					in_( "?~ %: expression" ) bgn_
						on_( ' ' )	command_do_( nop, same )
						on_( '\t' )	command_do_( nop, same )
						on_( '\n' )	command_do_( set_condition_to_contrary, same )
								command_do_( push_condition_from_expression, base )
						on_other	command_do_( error, base )
						end
	
				in_( "?~ %[" ) bgn_
					on_any	command_do_( evaluate_expression, "?~ %[_" )
					end
					in_( "?~ %[_" ) bgn_
						on_( ']' )	command_do_( nop, "?~ %[_]" )
						on_other	command_do_( error, base )
						end
						in_( "?~ %[_]" ) bgn_
							on_( ' ' )	command_do_( nop, same )
							on_( '\t' )	command_do_( nop, same )
							on_( '\n' )	command_do_( set_condition_to_contrary, same )
									command_do_( push_condition_from_expression, base )
							on_other	command_do_( error, base )
									end
				in_( "?~ %variable" ) bgn_
					on_( ' ' )	command_do_( nop, same )
					on_( '\t' )	command_do_( nop, same )
					on_( '\n' )	command_do_( set_condition_to_contrary, same )
							command_do_( push_condition_from_variable, base )
					on_other	command_do_( error, base )
					end

		in_( "? identifier" ) bgn_
			on_( ' ' )	command_do_( nop, same )
			on_( '\t' )	command_do_( nop, same )
			on_( '\n' )	command_do_( error, base )
			on_( ':' )	command_do_( nop, "? identifier:" )
			on_other	command_do_( error, base )
			end
			in_( "? identifier:" ) bgn_
				on_( ' ' )	command_do_( nop, same )
				on_( '\t' )	command_do_( nop, same )
				on_( '%' )	command_do_( nop, "? identifier: %" )
				on_( '\n' )	command_do_( error, base )
				on_other	command_do_( evaluate_expression, "? identifier: expression" )
				end
				in_( "? identifier: expression" ) bgn_
					on_( ' ' )	command_do_( nop, same )
					on_( '\t' )	command_do_( nop, same )
					on_( '\n' )	command_do_( push_loop, base )
					on_other	command_do_( error, base )
					end
				in_( "? identifier: %" ) bgn_
					on_( '[' )	command_do_( nop, "? identifier: %[" )
					on_other	command_do_( error, base )
					end
					in_( "? identifier: %[" ) bgn_
						on_any	command_do_( evaluate_expression, "? identifier: %[_" )
						end
						in_( "? identifier: %[_" ) bgn_
							on_( ']' )	command_do_( nop, "? identifier: %[_]" )
							on_other	command_do_( error, base )
							end
							in_( "? identifier: %[_]" ) bgn_
								on_( ' ' )	command_do_( nop, same )
								on_( '\t' )	command_do_( nop, same )
								on_( '\n' )	command_do_( push_loop, base )
								on_other	command_do_( error, base )
								end
	in_( ":" ) bgn_
		on_( ' ' )	command_do_( nop, same )
		on_( '\t' )	command_do_( nop, same )
		on_( '\n' )	command_do_( error, base )
		on_( '%' )	command_do_( nop, ": %" )
		on_( '<' )	command_do_( nop, ":<" )
		on_( '~' )	command_do_( nop, ":~" )
		on_other	command_do_( read_0, ": identifier" )
		end
		in_( ": identifier" ) bgn_
			on_( ' ' )	command_do_( nop, same )
			on_( '\t' )	command_do_( nop, same )
			on_( '\n' )	command_do_( error, base )
			on_( ':' )	command_do_( nop, ": identifier :" )
			on_other	command_do_( error, base )
			end
			in_( ": identifier :" ) bgn_
				on_( ' ' )	command_do_( nop, same )
				on_( '\t' )	command_do_( nop, same )
				on_( '%' )	command_do_( nop, ": identifier : %" )
				on_( '\n' )	command_do_( error, base )
				on_( '!' )	command_do_( nop, ": identifier : !" )
				on_other	command_do_( read_expression, ": identifier : expression" )
				end
				in_( ": identifier : expression" ) bgn_
					on_( ' ' )	command_do_( nop, same )
					on_( '\t' )	command_do_( nop, same )
					on_( '(' )	command_do_( set_results_to_nil, ": identifier : narrative(" )
					on_( '\n' )	command_do_( assign_expression, RETURN )
					on_other	command_do_( error, base )
					end
				in_( ": identifier : %" ) bgn_
					on_( '[' )	command_do_( nop, ": identifier : %[" )
					on_other	command_do_( read_expression, ": identifier : expression" )
					end
					in_( ": identifier : %[" ) bgn_
						on_any	command_do_( evaluate_expression, ": identifier : %[_" )
						end
						in_( ": identifier : %[_" ) bgn_
							on_( ' ' )	command_do_( nop, same )
							on_( '\t' )	command_do_( nop, same )
							on_( ']' )	command_do_( nop, ": identifier : %[_]" )
							on_other	command_do_( error, base )
							end
							in_( ": identifier : %[_]" ) bgn_
								on_( ' ' )	command_do_( nop, same )
								on_( '\t' )	command_do_( nop, same )
								on_( '\n' )	command_do_( assign_results, RETURN )
								on_( '.' )	command_do_( nop, ": identifier : %[_]." )
								on_other	command_do_( error, base )
								end
								in_( ": identifier : %[_]." ) bgn_
									on_( '$' )	command_do_( read_va, ": identifier : %[_].$" )
									on_other	command_do_( read_1, ": identifier : %[_].narrative" )
									end
									in_( ": identifier : %[_].$" ) bgn_
										on_( '\n' )	command_do_( assign_va, RETURN )
										on_other	command_do_( error, base )
										end
									in_( ": identifier : %[_].narrative" ) bgn_
										on_( ' ' )	command_do_( nop, same )
										on_( '\t' )	command_do_( nop, same )
										on_( '(' )	command_do_( nop, ": identifier : narrative(" )
										on_other	command_do_( error, base )
										end
				in_( ": identifier : !" ) bgn_
					on_( '!' )	command_do_( set_expression_mode, same )
							command_do_( read_expression, ": identifier : !." )
					on_( '~' )	command_do_( set_expression_mode, same )
							command_do_( read_expression, ": identifier : !." )
					on_( '*' )	command_do_( set_expression_mode, same )
							command_do_( read_expression, ": identifier : !." )
					on_( '_' )	command_do_( set_expression_mode, same )
							command_do_( read_expression, ": identifier : !." )
					on_other	command_do_( error, base )
					end
					in_( ": identifier : !." ) bgn_
						on_any	command_do_( expression_op, ": identifier : !. expression" )
						end
						in_( ": identifier : !. expression" ) bgn_
							on_( '(' )	command_do_( set_results_to_nil, ": identifier : !. narrative(" )
							on_( '\n' )	command_do_( assign_results, RETURN )
							on_other	command_do_( error, base )
							end
						in_( ": identifier : !. narrative(" ) bgn_
							on_( ' ' )	command_do_( nop, same )
							on_( '\t' )	command_do_( nop, same )
							on_( ')' )	command_do_( nop, ": identifier : !. narrative(_)" )
							on_other	command_do_( error, base )
							end
							in_( ": identifier : !. narrative(_)" ) bgn_
								on_( ' ' )	command_do_( nop, same )
								on_( '\t' )	command_do_( nop, same )
								on_( '\n' )	command_do_( narrative_op, same )
										command_do_( assign_narrative, RETURN )
								on_other	command_do_( error, base )
								end
				in_( ": identifier : narrative(" ) bgn_
					on_( ' ' )	command_do_( nop, same )
					on_( '\t' )	command_do_( nop, same )
					on_( ')' )	command_do_( nop, ": identifier : narrative(_)" )
					on_other	command_do_( error, base )
					end
					in_( ": identifier : narrative(_)" ) bgn_
						on_( ' ' )	command_do_( nop, same )
						on_( '\t' )	command_do_( nop, same )
						on_( '\n' )	command_do_( assign_narrative, RETURN )
						on_other	command_do_( error, base )
						end

		in_( ": %" ) bgn_
			on_( '.' )	command_do_( set_results_to_nil, ": %[_]." )
			on_( '[' )	command_do_( nop, ": %[" )
			on_other	command_do_( error, base )
			end
			in_( ": %[" ) bgn_
				on_any	command_do_( evaluate_expression, ": %[_" )
				end
				in_( ": %[_" ) bgn_
					on_( ']' )	command_do_( nop, ": %[_]" )
					on_other	command_do_( error, base )
					end
					in_( ": %[_]" ) bgn_
						on_( '.' )	command_do_( nop, ": %[_]." )
						on_other	command_do_( error, base )
						end
			in_( ": %[_]." ) bgn_
				on_( '$' )	command_do_( read_va, ": %[_].$" )
				on_other	command_do_( error, base )
				end
				in_( ": %[_].$" ) bgn_
					on_( ' ' )	command_do_( nop, same )
					on_( '\t' )	command_do_( nop, same )
					on_( ':' )	command_do_( nop, ": %[_].$:" )
					on_other	command_do_( error, base )
					end
					in_( ": %[_].$:" ) bgn_
						on_( ' ' )	command_do_( nop, same )
						on_( '\t' )	command_do_( nop, same )
						on_( '%' )	command_do_( nop, ": %[_].$: %" )
						on_other	command_do_( read_1, ": %[_].$: arg" )
						end
						in_( ": %[_].$: %" ) bgn_
							on_any	command_do_( read_1, ": %[_].$: %arg" )
							end
							in_( ": %[_].$: %arg" ) bgn_
								on_( ' ' )	command_do_( nop, same )
								on_( '\t' )	command_do_( nop, same )
								on_( '\n' )	command_do_( set_va_from_variable, RETURN )
								on_other	command_do_( error, base )
								end
						in_( ": %[_].$: arg" ) bgn_
							on_( ' ' )	command_do_( nop, same )
							on_( '\t' )	command_do_( nop, same )
							on_( '(' )	command_do_( nop, ": %[_].$: arg(" )
							on_( '\n' )	command_do_( set_va, RETURN )
							on_other	command_do_( error, base )
							end
							in_( ": %[_].$: arg(" ) bgn_
								on_( ' ' )	command_do_( nop, same )
								on_( '\t' )	command_do_( nop, same )
								on_( ')' )	command_do_( nop, ": %[_].$: arg(_)" )
								on_other	command_do_( error, base )
								end
								in_( ": %[_].$: arg(_)" ) bgn_
									on_( ' ' )	command_do_( nop, same )
									on_( '\t' )	command_do_( nop, same )
									on_( '\n' )	command_do_( set_va, RETURN )
									on_other	command_do_( error, base )
									end

		in_( ":~" ) bgn_
			on_( ' ' )	command_do_( nop, same )
			on_( '\t' )	command_do_( nop, same )
			on_( '\n' )	command_do_( reset_variables, RETURN )
			on_other	command_do_( read_0, ":~ identifier" )
			end
			in_( ":~ identifier" )
				if ( context->narrative.mode.action.one ) bgn_
					on_( ' ' )	command_do_( variable_reset, out )
					on_( '\t' )	command_do_( variable_reset, out )
					on_( '\n' )	command_do_( variable_reset, out )
					on_other	command_do_( nothing, out )
					end
				else bgn_
					on_( ' ' )	command_do_( nop, same )
					on_( '\t' )	command_do_( nop, same )
					on_( '\n' )	command_do_( variable_reset, base )
					on_other	command_do_( error, base )
					end
		in_( ":<" ) bgn_
			on_( ' ' )	command_do_( nop, same )
			on_( '\t' )	command_do_( nop, same )
			on_( '%' )	command_do_( nop, ":< %" )
			on_other	command_do_( error, base )
			end
			in_( ":< %" ) bgn_
				on_( '(' )	command_do_( nop, ":< %(" )
				on_other	command_do_( error, base )
				end
				in_( ":< %(" ) bgn_
					on_( ' ' )	command_do_( nop, same )
					on_( '\t' )	command_do_( nop, same )
					on_other	command_do_( read_0, ":< %( string" )
					end
					in_( ":< %( string" ) bgn_
						on_( ' ' )	command_do_( nop, same )
						on_( '\t' )	command_do_( nop, same )
						on_( ')' )	command_do_( nop, ":< %( string )" )
						on_other	command_do_( error, base )
						end
						in_( ":< %( string )" )
							if ( context->narrative.mode.action.one ) bgn_
								on_( ' ' )	command_do_( input_command, out )
								on_( '\t' )	command_do_( input_command, out )
								on_( '\n' )	command_do_( input_command, out )
								on_other	command_do_( nothing, out )
								end
							else bgn_
								on_( ' ' )	command_do_( nop, same )
								on_( '\t' )	command_do_( nop, same )
								on_( '\n' )	command_do_( input_command, base )
								on_other	command_do_( error, base )
								end
	end

	}
	while ( strcmp( state, "" ) );

	return event;
}

