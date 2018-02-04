#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "database.h"
#include "registry.h"
#include "kernel.h"

#include "api.h"
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

static char *loop = "loop";
static char *clause = "clause";
static char *otherwise = "~";

static _action command_push;
static _action command_pop;
static _action command_pop_input;
static _action command_err;
static _action check_out;
static _action set_clause_to_contrary;
static _action clause_else;
static _action clause_end;
static _action push_input_hcn;

/*---------------------------------------------------------------------------
	read_command
---------------------------------------------------------------------------*/
#define do_( a, s ) \
	{ addItem( &actions, &a ); addItem( &states, s ); }
/*
	These macros return control to narrative single-action editing mode
	1. BRKERR rejects loop or test initialization instructions
	2. BRKOUT allows to record instructions not followed by '\n'
*/
#define BRKERR if ( context->command.one ) do_( error, "" ) else
#define BRKOUT if ( context->command.one ) do_( nothing, "" ) else
/*
	The following macro is used to prevent the user from issueing
	the internal command '>..:' then possibly without sync (|)
	The user command
		>@session:< whatever
	requires the SENDER to keep the socket connection opened until
	the response is sent (otherwise the RECEIVER will crash writing
	on a closed socket connection).
	To achieve this purpose, it translates internally into:
		>@session:>..: whatever |
	which 1) requires the receiver to write back the response, and
	2) blocks the sender until the response is received.
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
	case -1:
		do_( command_err, base )
		break;
	case 0: // input() reached EOF
		// pop_state is when pop() command failed aka.
		// " attempt to pop beyond authorized level"
		bgn_
		in_( pop_state )	do_( command_pop_input, base )
		in_other		do_( command_pop_input, same )
		end
		break;
	case ' ':
	case '\t':
		// in general, we want to ignore ' ' and '\t'
		// but there are special cases...
		switch( state[ strlen(state) - 1 ] ) {
		case '$':
		case '%':
			do_( command_err, base )
			break;
		default:
			bgn_
			in_( ">:" )			do_( output_char, same )
			in_( ">: \\" )			do_( output_special_char, ">:" )
			in_( ">: \"" )			do_( output_char, same )
			in_( ">: \"\\" )		do_( output_special_char, ">: \"" )
			in_( ">: %variable" )		do_( output_variable, ">:" )
			in_other			do_( nop, same )
			end
		}
		break;
	default:
		// special events 0, -1, ' ', '\t' have been handled above.
		// other events are handled below, depending on state:
		bgn_
		in_( base ) bgn_
			on_( '\n' )	do_( nop, same )
			on_( ':' )	do_( nop, ":" )
			on_( '>' )	do_( nop, ">" )
			on_( '<' )	do_( nop, "<" )
			on_( '!' )	do_( nop, "!" )
			on_( '~' )	do_( nop, "~" )
			on_( '?' )	BRKERR do_( nop, "?" )
			on_( '/' )	BRKERR do_( nop, "/" )
			on_( '|' )	do_( sync_delivery, same )
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
					on_( ':' )	do_( nop, "< cgi >:" )
					on_other	do_( command_err, base )
					end
				in_( "< cgi >:" ) bgn_
					on_any do_( input_cgi, base )
					end
			in_( "< session:" ) bgn_
				on_any	do_( read_path, "< session://path" )
				end
				in_( "< session://path" ) bgn_
					on_( '>' )	do_( nop, "< session://path >" )
					on_other	do_( command_err, base )
					end
				in_( "< session://path >" ) bgn_
					on_( ':' )	do_( nop, "< session://path >:" )
					on_other	do_( command_err, base )
					end
				in_( "< session://path >:" ) bgn_
					on_any	do_( read_2, "< session://path >: event" )
					end
				in_( "< session://path >: event" ) bgn_
					on_( '@' )	do_( nop, "< session://path >: event @" )
					on_( '~' )	do_( nop, "< session://path >: event ~" )
					on_other	do_( command_err, base )
					end
				in_( "< session://path >: event @" ) bgn_
					on_( '\n' )	do_( handle_iam, base )
					on_other	do_( command_err, base )
					end
				in_( "< session://path >: event ~" ) bgn_
					on_( '\n' )	do_( handle_bye, base )
					on_other	do_( command_err, base )
					end
		in_( "exit" ) bgn_
			on_( '\n' )	do_( exit_command, out )
			on_other	BRKOUT do_( command_err, base )
			end
		in_( "/" ) bgn_
			on_( '\n' )	do_( command_pop, pop_state )
			on_( '~' )	do_( command_pop, pop_state )
			on_( '.' )	do_( nop, "/." )
			on_other	do_( command_err, base )
			end
			in_( "/." ) bgn_
				on_( '\n' )	do_( command_pop, pop_state )
				on_other	do_( command_err, base )
				end
		in_( "?" ) bgn_
			on_( ':' )	do_( nop, "? identifier:" )
			on_( '!' )	do_( nop, "?!" )
			on_( '~' )	do_( nop, "?~" )
			on_other	do_( command_err, base )
			end
			in_( "? identifier" ) bgn_
				on_( ':' )	do_( nop, "? identifier:" )
				on_other	do_( command_err, base )
				end
				in_( "? identifier:" ) bgn_
					on_any	do_( evaluate_expression, "? identifier: expression" )
					end
					in_( "? identifier: expression" ) bgn_
						on_( '\n' )	do_( command_push, loop )
						on_other	do_( command_err, base )
						end
			in_( "?!" ) bgn_
				on_( ':' )	do_( nop, "?!:" )
				on_other	do_( command_err, base )
				end
				in_( "?!:" ) bgn_
					on_any	do_( test_expression, "?!: expression" )
					end
					in_( "?!: expression" ) bgn_
						on_( '\n' )	do_( command_push, clause )
						on_other	do_( command_err, base )
						end
			in_( "?~" ) bgn_
				on_( '!' )	do_( nop, "?~!" )
				on_other	do_( command_err, base )
				end
				in_( "?~!" ) bgn_
					on_( ':' )	do_( nop, "?~!:" )
					on_other	do_( command_err, base )
					end
					in_( "?~!:" ) bgn_
						on_any	do_( test_expression, "?~!: expression" )
						end
						in_( "?~!: expression" ) bgn_
							on_( '\n' )	do_( set_clause_to_contrary, same )
									do_( command_push, clause )
							on_other	do_( command_err, base )
							end
			in_( "?!:_/" ) bgn_
				on_( '~' )	do_( clause_else, "?!:_/~" )
				on_other	do_( clause_end, base )
				end
				in_( "?!:_/~" ) bgn_
					on_( '?' )	do_( nop, "?!:_/~?" )
					on_( '\n' )	do_( command_push, otherwise )
					on_other	do_( command_err, base )
					end
				in_( "?!:_/~?" ) bgn_
					on_( '!' ) 	do_( nop, "?!" )
					on_( '~' )	do_( nop, "?~" )
					on_other	do_( command_err, base )
					end
		in_( "~" ) bgn_
			on_( '.' )	do_( nop, "~." )
			on_other	do_( command_err, base )
			end
			in_( "~." ) bgn_
				on_( '\n' )	do_( nop, out )
				on_other	BRKOUT do_( command_err, base )
				end
		in_( "!" ) bgn_
			on_( '!' )	do_( set_command_op, "!." )
			on_( '~' )	do_( set_command_op, "!." )
			on_( '*' )	do_( set_command_op, "!." )
			on_( '_' )	do_( set_command_op, "!." )
			on_other	do_( command_err, base )
			end
			in_( "!." ) bgn_
				on_( '$' )	do_( nop, "!. $" )
				on_( '<' )	do_( nop, "!. <" )
#if 0
				on_other	do_( build_expression, "!. expression" )
#else
				on_other	do_( assimilate_expression, "!. expression" )
#endif
				end
				in_( "!. expression" ) bgn_
					on_( '\n' )	do_( expression_op, out )
					on_other	BRKOUT do_( command_err, base )
					end
				in_( "!. $" ) bgn_
					on_( '[' )	do_( nop, "!. $[" )
					on_( '.' )	do_( set_results_to_nil, "!. $[_]." )
					on_other	do_( command_err, base )
					end
					in_( "!. $[" ) bgn_
						on_any	do_( solve_expression, "!. $[_" )
						end
					in_( "!. $[_" ) bgn_
						on_( ']' )	do_( nop, "!. $[_]" )
						on_other	do_( command_err, base )
						end
					in_( "!. $[_]" ) bgn_
						on_( '.' )	do_( nop, "!. $[_]." )
						on_other	do_( command_err, base )
						end
					in_( "!. $[_]." ) bgn_
						on_any	do_( read_1, "!. $[_].narrative" )
						end
					in_( "!. $[_].narrative" ) bgn_
						on_( '(' )	do_( nop, "!. $[_].narrative(" )
						on_other	do_( command_err, base )
						end
					in_( "!. $[_].narrative(" ) bgn_
						on_( ')' )	do_( nop, "!. $[_].narrative(_)" )
						on_other	do_( command_err, base )
						end
					in_( "!. $[_].narrative(_)" ) bgn_
						on_( '\n' )	do_( narrative_op, out )
						on_other	BRKOUT do_( command_err, base )
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
		in_( ">" ) bgn_
			on_( '@' )	do_( nop, ">@" )
			on_( ':' )	do_( set_output_target, ">:" )
			on_( '.' )	do_( nop, "> ." )
			on_( '<' )	do_( nop, "><" )
			on_other	do_( read_0, "> identifier" )
			end
			in_( ">@" ) bgn_
				on_any	do_( read_path, ">@session" )
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
				on_( ':' )	do_( set_output_target, ">:" )
				on_other	do_( command_err, base )
				end
			in_( "><" ) bgn_
				on_any	do_( read_token, "file\0>< file" )
				end
				in_( ">< file" ) bgn_
					on_( ':' )	do_( nop, ">< file:" )
					on_other	do_( command_err, base )
					end
				in_( ">< file:" ) bgn_
					on_any	do_( read_path, ">< file:path" )
					end
				in_( ">< file:path" ) bgn_
					on_( '>' )	do_( nop, ">< file:path >" )
					on_other	do_( command_err, base )
					end
				in_( ">< file:path >" ) bgn_
					on_( ':' )	do_( set_output_target, ">:" )
					on_other	do_( command_err, base )
					end
		in_( ">:" ) bgn_
			on_( '\n' )	do_( output_char, out )
			on_( '|' )	do_( sync_output, ">:_ |" )
			on_( '\\' )	do_( nop, ">: \\" )
			on_( '$' )	do_( nop, ">: $" )
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
			in_( ">: $" ) bgn_
				on_( '{' )	do_( nop, ">: ${" )
				on_( '.' )	do_( set_results_to_nil, ">: $." )
				on_( '[' )	do_( nop, ">: $[" )
				on_( '<' )	do_( nop, ">: $<" )
				on_( '\\' )	do_( nop, ">: $\\" )
				on_( '(' )	do_( set_results_to_nil, ">: $[_](" )
				on_other	do_( output_dol, ">:" )
				end
				in_( ">: ${" ) bgn_
					on_any	do_( evaluate_expression, ">: ${_" )
					end
					in_( ">: ${_" ) bgn_
						on_( '}' )	do_( output_results, ">:" )
						on_other	do_( command_err, base )
						end
				in_( ">: $." ) bgn_
					on_( '[' )	do_( nop, ">: $.[" )
					on_( '<' )	do_( nop, ">: $.<" )
					on_other	do_( read_1, ">: $[_].narrative" )
					end
					in_( ">: $.[" ) bgn_
						on_any	do_( evaluate_expression, ">: $.[_" )
						end
						in_( ">: $.[_" ) bgn_
							on_( ']' )	do_( output_as_is, ">:" )
							on_other	do_( command_err, base )
							end
					in_( ">: $.<" ) bgn_
						on_any	do_( read_scheme, ">: $.< scheme" )
						end
						in_( ">: $.< scheme" ) bgn_
							on_( ':' )	do_( nop, ">: $.< scheme:" )
							on_( '>' )	do_( nop, ">: $.<_>" )
							on_other	do_( command_err, base )
							end
						in_( ">: $.< scheme:" ) bgn_
							on_any	do_( read_path, ">: $.<_" )
							end
							in_( ">: $.<_" ) bgn_
								on_( '>' )	do_( output_scheme_contents, ">:" )
								on_other	do_( command_err, base )
								end
				in_( ">: $[" ) bgn_
					on_any	do_( solve_expression, ">: $[_" )
					end
					in_( ">: $[_" ) bgn_
						on_( ']' )	do_( nop, ">: $[_]" )
						on_other	do_( command_err, base )
						end
					in_( ">: $[_]" ) bgn_
						on_( '.' )	do_( nop, ">: $[_]." )
						on_( '(' )	do_( nop, ">: $[_](" )
						on_other	do_( command_err, base )
						end
					in_( ">: $[_]." ) bgn_
						on_any	do_( read_1, ">: $[_].narrative" )
						end
						in_( ">: $[_].narrative" ) bgn_
							on_( '(' )	do_( nop, ">: $[_].narrative(" )
							on_other	do_( command_err, base )
							end
						in_( ">: $[_].narrative(" ) bgn_
							on_( ')' )	do_( output_narratives, ">:" )
							on_other	do_( command_err, base )
							end
					in_( ">: $[_](" ) bgn_
						on_any		do_( read_va, ">: $[_](_" )
						end
						in_( ">: $[_](_" ) bgn_
							on_( ')' )	do_( output_vas, ">:" )
							on_other	do_( command_err, base )
							end
				in_( ">: $<" ) bgn_
					on_any	do_( read_scheme, ">: $< scheme" )
					end
					in_( ">: $< scheme" ) bgn_
						on_( ':' )	do_( nop, ">: $< scheme:" )
						on_other	do_( command_err, base )
						end
					in_( ">: $< scheme:" ) bgn_
						on_any	do_( read_path, ">: $<_" )
						end
						in_( ">: $<_" ) bgn_
							on_( '>' )	do_( nop, ">: $<_>" )
							on_other	do_( command_err, base )
							end
					in_( ">: $<_>" ) bgn_
						on_( '<' )	do_( nop, ">: $<_><" )
						on_other	do_( command_err, base )
						end
					in_( ">: $<_><" ) bgn_
						on_( '/' )	do_( nop, ">: $<_></" )
						on_other	do_( command_err, base )
						end
					in_( ">: $<_></" ) bgn_
						on_( '>' )	do_( push_input_hcn, base )
						on_other	do_( command_err, base )
						end
			in_( ">: %" ) bgn_
				on_( '?' )	do_( output_variator, ">:" )
				on_( '<' )	do_( nop, ">: %<" )
				on_( '[' )	do_( nop, ">: %[" )
				on_( '\'' )	do_( output_special_quote, ">:" )
				on_separator	do_( output_mod, ">:" )
				on_other	do_( read_1, ">: %variable" )
				end
				in_( ">: %<" ) bgn_
					on_any	do_( read_scheme, ">: %< scheme" )
					end
					in_( ">: %< scheme" ) bgn_
						on_( ':' )	do_( nop, ">: %< scheme:" )
						on_other	do_( command_err, base )
						end
					in_( ">: %< scheme:" ) bgn_
						on_any	do_( read_path, ">: %<_" )
						end
						in_( ">: %<_" ) bgn_
							on_( '>' )	do_( output_scheme_value, ">:" )
							on_other	do_( command_err, base )
							end
				in_( ">: %[" ) bgn_
					on_any	do_( solve_expression, ">: %[_" )
					end
					in_( ">: %[_" ) bgn_
						on_( ']' )	do_( output_results, ">:" )
						on_other	do_( command_err, base )
						end
				in_( ">: %variable" ) bgn_
					on_any	do_( output_variable, ">:" )
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
					on_other	do_( read_0, ">:_ | > variable" )
					end
					in_( ">:_ | > variable" ) bgn_
						on_( ':' )	do_( pipe_output_to_variable, ">:_ | >:" )
						on_other	do_( command_err, base )
						end
					in_( ">:_ | >:" ) bgn_
						on_( '\n' )	do_( nop, out )
						on_other	do_( nothing, out )
						end
		in_( ":" ) bgn_
			on_( '\n' )	do_( command_err, base )
			on_( '$' )	do_( nop, ": $" )
			on_( '<' )	do_( nop, ":<" )
			on_( '~' )	do_( nop, ":~" )
			on_other	do_( read_0, ": variable" )
			end
			in_( ":~" ) bgn_
				on_( '\n' )	do_( reset_variables, out )
				on_( '$' )	do_( nop, ":~ $" )
				on_other	do_( read_0, ":~ identifier" )
				end
				in_( ":~ identifier" ) bgn_
					on_( '\n' )	do_( reset_variable, out )
					on_other	BRKOUT do_( command_err, base )
					end
				in_( ":~ $" ) bgn_
					on_( '(' )	do_( set_results_to_nil, ":~ $[_](" )
					on_( '[' )	do_( nop, ":~ $[" )
					on_other	do_( command_err, base )
					end
					in_( ":~ $[" ) bgn_
						on_any	do_( solve_expression, ":~ $[_" )
						end
					in_( ":~ $[_" ) bgn_
						on_( ']' )	do_( nop, ":~ $[_]" )
						on_other	do_( command_err, base )
						end
					in_( ":~ $[_]" ) bgn_
						on_( '(' )	do_( nop, ":~ $[_](" )
						on_other	do_( command_err, base )
						end
					in_( ":~ $[_](" ) bgn_
						on_any	do_( read_va, ":~ $[_](_" )
						end
					in_( ":~ $[_](_" ) bgn_
						on_( ')' )	do_( nop, ":~ $[_](_)" )
						on_other	do_( command_err, base )
						end
					in_( ":~ $[_](_)" ) bgn_
						on_( '\n' )	do_( reset_va, out )
						on_other	BRKOUT do_( command_err, base )
						end
		in_( ": $" ) bgn_
			on_( '[' )	do_( nop, ": $[" )
			on_( '(' )	do_( set_results_to_nil, ": $[_](" )
			on_other	do_( command_err, base )
			end
			in_( ": $[" ) bgn_
				on_any	do_( solve_expression, ": $[_" )
				end
				in_( ": $[_" ) bgn_
					on_( ']' )	do_( nop, ": $[_]" )
					on_other	do_( command_err, base )
					end
				in_( ": $[_]" ) bgn_
					on_( '(' )	do_( nop, ": $[_](" )
					on_other	do_( command_err, base )
					end
				in_( ": $[_](" ) bgn_
					on_any	do_( read_va, ": $[_](_" )
					end
				in_( ": $[_](_" ) bgn_
					on_( ')' )	do_( nop, ": $[_](_)" )
					on_other	do_( command_err, base )
					end
				in_( ": $[_](_)" ) bgn_
					on_( ':' )	do_( nop, ": $[_](_) :" )
					on_other	do_( command_err, base )
					end
				in_( ": $[_](_) :" ) bgn_
					on_( '%' )	do_( nop, ": $[_](_) : %" )
					on_other	do_( read_1, ": $[_](_) : arg" )
					end
				in_( ": $[_](_) : %" ) bgn_
					on_any	do_( read_1, ": $[_](_) : %arg" )
					end
			in_( ": $[_](_) : %arg" ) bgn_
				on_( '\n' )	do_( set_va_from_variable, out )
				on_other	BRKOUT do_( command_err, base )
				end
			in_( ": $[_](_) : arg" ) bgn_
				on_( '\n' )	do_( set_va, out )
				on_( '(' )	do_( nop, ": $[_](_) : narrative(" )
				on_other	BRKOUT do_( command_err, base )
				end
			in_( ": $[_](_) : narrative(" ) bgn_
				on_( ')' )	do_( nop, ": $[_](_) : narrative(_)" )
				on_other	do_( command_err, base )
				end
				in_( ": $[_](_) : narrative(_)" ) bgn_
					on_( '\n' )	do_( set_va_from_narrative, out )
					on_other	BRKOUT do_( command_err, base )
					end
		in_( ": variable" ) bgn_
			on_( '+' )	do_( set_assignment_mode, ": variable +" )
			on_( ':' )	do_( set_assignment_mode, ": variable :" )
			on_( '<' )	do_( set_assignment_mode, ": variable <" )
			on_other	do_( command_err, base )
			end
			in_( ": variable +" ) bgn_
				on_( ':' )	do_( nop, ": variable :" )
				on_( '<' )	do_( nop, ": variable <" )
				on_other	do_( command_err, base )
				end
			in_( ": variable <" ) bgn_
				on_any	do_( dereference_expression, ": variable < expression" )
				end
				in_( ": variable < expression" ) bgn_
					on_( '\n' )	do_( assign_results, out )
					on_other	BRKOUT do_( command_err, base )
					end
			in_( ": variable :" ) bgn_
				on_( '$' )	do_( nop, ": variable : $" )
				on_( '!' )	do_( nop, ": variable : !" )
				on_other	do_( assimilate_expression, ": variable : expression" )
				end
				in_( ": variable : expression" ) bgn_
					on_( '\n' )	do_( assign_expression, out )
					on_other	BRKOUT do_( command_err, base )
					end
				in_( ": variable : $" ) bgn_
					on_( '[' )	do_( nop, ": variable : $[" )
					on_( '{' )	do_( nop, ": variable : ${" )
					on_( '.' )	do_( set_results_to_nil, ": variable : $[_]." )
					on_( '(' )	do_( set_results_to_nil, ": variable : $[_](" )
					on_other	do_( command_err, base )
					end
				in_( ": variable : ${" ) bgn_
					on_any	do_( evaluate_expression, ": variable : ${_" )
					end
					in_( ": variable : ${_" ) bgn_
						on_( '}' )	do_( nop, ": variable : ${_}" )
						on_other	do_( command_err, base )
						end
					in_( ": variable : ${_}" ) bgn_
						on_( '\n' )	do_( assign_literals, out )
						on_other	do_( command_err, base )
						end
				in_( ": variable : $[" ) bgn_
					on_any	do_( solve_expression, ": variable : $[_" )
					end
					in_( ": variable : $[_" ) bgn_
						on_( ']' )	do_( nop, ": variable : $[_]" )
						on_other	do_( command_err, base )
						end
					in_( ": variable : $[_]" ) bgn_
						on_( '.' )	do_( nop, ": variable : $[_]." )
						on_( '(' )	do_( nop, ": variable : $[_](" )
						on_other	do_( command_err, base )
						end
					in_( ": variable : $[_]." ) bgn_
						on_any	do_( read_1, ": variable : $[_].narrative" )
						end
						in_( ": variable : $[_].narrative" ) bgn_
							on_( '(' )	do_( nop, ": variable : $[_].narrative(" )
							on_other	do_( command_err, base )
							end
						in_( ": variable : $[_].narrative(" ) bgn_
							on_( ')' )	do_( nop, ": variable : $[_].narrative(_)" )
							on_other	do_( command_err, base )
							end
						in_( ": variable : $[_].narrative(_)" ) bgn_
							on_( '\n' )	do_( assign_narrative, out )
							on_other	BRKOUT do_( command_err, base )
							end
					in_( ": variable : $[_](" ) bgn_
						on_any	do_( read_va, ": variable : $[_](_" )
						end
						in_( ": variable : $[_](_" ) bgn_
							on_( ')' )	do_( nop, ": variable : $[_](_)" )
							on_other	do_( command_err, base )
							end
						in_( ": variable : $[_](_)" ) bgn_
							on_( '\n' )	do_( assign_va, out )
							on_other	BRKOUT do_( command_err, base )
							end
				in_( ": variable : !" ) bgn_
					on_( '!' )	do_( set_command_op, ": variable : !." )
					on_( '~' )	do_( set_command_op, ": variable : !." )
					on_( '*' )	do_( set_command_op, ": variable : !." )
					on_( '_' )	do_( set_command_op, ": variable : !." )
					on_other	do_( command_err, base )
					end
					in_( ": variable : !." ) bgn_
						on_( '$' )	do_( nop, ": variable : !. $" )
#if 0
						on_other	do_( build_expression, ": variable : !. expression" )
#else
						on_other	do_( assimilate_expression, ": variable : !. expression" )
#endif
						end
						in_( ": variable : !. expression" ) bgn_
							on_( '\n' )	do_( expression_op, same )
									do_( assign_results, out )
							on_other	BRKOUT do_( command_err, base )
							end
						in_( ": variable : !. $" ) bgn_
							on_( '[' )	do_( nop, ": variable : !. $[" )
							on_( '.' )	do_( set_results_to_nil, ": variable : !. $[_]." )
							on_other	do_( command_err, base )
							end
							in_( ": variable : !. $[" ) bgn_
								on_any	do_( solve_expression, ": variable : !. $[_" )
								end
							in_( ": variable : !. $[_" ) bgn_
								on_( ']' )	do_( nop, ": variable : !. $[_]" )
								on_other	do_( command_err, base )
								end
							in_( ": variable : !. $[_]" ) bgn_
								on_( '.' )	do_( nop, ": variable : !. $[_]." )
								on_other	do_( command_err, base )
								end
							in_( ": variable : !. $[_]." ) bgn_
								on_any	do_( read_1, ": variable : !. $[_].narrative" )
								end
							in_( ": variable : !. $[_].narrative" ) bgn_
								on_( '(' )	do_( nop, ": variable : !. $[_].narrative(" )
								on_other	do_( command_err, base )
								end
							in_( ": variable : !. $[_].narrative(" ) bgn_
								on_( ')' )	do_( nop, ": variable : !. $[_].narrative(_)" )
								on_other	do_( command_err, base )
								end
							in_( ": variable : !. $[_].narrative(_)" ) bgn_
								on_( '\n' )	do_( narrative_op, same )
										do_( assign_narrative, out )
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

/*---------------------------------------------------------------------------
	clause actions
---------------------------------------------------------------------------*/
static int
set_clause_to_contrary( char *state, int event, char **next_state, _context *context )
{
	context->clause.contrary = ~context->clause.contrary;
	return 0;
}
static int
clause_else( char *state, int event, char **next_state, _context *context )
{
	switch ( context->clause.mode ) {
	case PassiveClause:
		context->clause.mode = ActiveClause;
		break;
	case ActiveClause:
	case PostActiveClause:
		context->clause.mode = PostActiveClause;
		break;
	default:
		return output( Error, "on '~': not in conditional execution mode" );
	}
	return 0;
}
static int
clause_end( char *state, int event, char **next_state, _context *context )
{
	context->clause.mode = ClauseNone;
	return event;
}

/*---------------------------------------------------------------------------
	command_push
---------------------------------------------------------------------------*/
static _action set_loop;
static _action set_clause;
static _action set_otherwise;

static int
command_push( char *state, int event, char **next_state, _context *context )
{
#ifdef DEBUG
	outputf( Debug, "command_push(): pushing from level %d", context->command.level );
#endif
	// special case: pushing a loop in interactive mode, but no result to loop over
	if ( command_mode( 0, 0, ExecutionMode ) && ( context->input.stack == NULL ) &&
	     !strcmp( *next_state, loop ) && ( context->expression.results == NULL ))
	{
		event = 0;	// nothing to push for in this case
	}
	else {
		event = push( state, event, &base, context );
		if ( !strcmp( *next_state, loop ) ) {
			event = set_loop( state, event, next_state, context );
		}
		else if ( !strcmp( *next_state, clause ) ) {
			event = set_clause( state, event, next_state, context );
		}
		else if ( !strcmp( *next_state, otherwise ) ) {
			event = set_otherwise( state, event, next_state, context );
		}
	}
	*next_state = base;
	return event;
}

static int
set_loop( char *state, int event, char **next_state, _context *context )
{
	if ( !command_mode( 0, 0, ExecutionMode ) )
		return 0;
	if ( context->expression.results == NULL ) {
		set_command_mode( FreezeMode, context );
		return 0;
	}
	int do_start = 1;
	if (( context->command.stack->next )) {
		StackVA *stack = context->command.stack->next->ptr;
		if (( stack->loop.begin )) {
			// here we are about to execute a loop within a loop
			stack = context->command.stack->ptr;
			stack->loop.begin = context->input.instruction->next;
			do_start = 0;
		}
	}
	if ( do_start ) {
		set_command_mode( InstructionMode, context );
		set_record_mode( RecordInstructionMode, event, context );
	}
	StackVA *stack = context->command.stack->ptr;
	stack->loop.candidates = context->expression.results;
	context->expression.results = NULL;
	int type = (( context->expression.mode == ReadMode ) ? LiteralResults : EntityResults );
	listItem *value = newItem( stack->loop.candidates->ptr );
	assign_variable( AssignSet, &variator_symbol, value, type, context );
	return 0;
}
static int
set_clause( char *state, int event, char **next_state, _context *context )
{
	StackVA *stack = context->command.stack->ptr;
	switch ( context->command.mode ) {
	case FreezeMode:
		stack->clause.mode = PassiveClause;
		break;
	case InstructionMode:
		stack->clause.mode = ActiveClause;
		break;
	case ExecutionMode:
		switch ( context->clause.mode ) {
		case ClauseNone:
		case ActiveClause:
			stack->clause.mode = ( context->expression.results == NULL ) ?
				( context->clause.contrary ? ActiveClause : PassiveClause ) :
				( context->clause.contrary ? PassiveClause : ActiveClause );
			if ( context->expression.mode == ReadMode ) {
				freeLiterals( &context->expression.results );
			}
			else freeListItem( &context->expression.results );
			break;
		default:
			stack->clause.mode = PostActiveClause;
		}
		if ( stack->clause.mode != ActiveClause )
			set_command_mode( FreezeMode, context );
	}
	context->clause.mode = ClauseNone;
	context->clause.contrary = 0;
	return 0;
}
static int
set_otherwise( char *state, int event, char **next_state, _context *context )
{
	if ( command_mode( 0, 0, ExecutionMode ) )
		if ( context->clause.mode != ActiveClause )
			set_command_mode( FreezeMode, context );

	context->clause.mode = ClauseNone;
	StackVA *stack = context->command.stack->ptr;
	stack->clause.mode = ClauseElse;
	return 0;
}

/*---------------------------------------------------------------------------
	command_pop
---------------------------------------------------------------------------*/
static _action pop_clause;
static _action pop_narrative;

static int
command_pop( char *state, int event, char **next_state, _context *context )
{
	StackVA *stack = (StackVA *) context->command.stack->ptr;
	if ( stack->clause.mode )
		return pop_clause( state, event, next_state, context );

	switch ( context->command.mode ) {
	case FreezeMode:
		break;
	case InstructionMode:
		if ( context->narrative.mode.editing || context->narrative.mode.output )
			return pop_narrative( state, event, next_state, context );
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
		if ( stack->loop.begin == NULL ) {
			// not in a loop
			if ( context->command.block ) {
				return pop_narrative( state, event, next_state, context );
			}
			break;
		}
		// popping during loop execution
		// -----------------------------
		if (( stack->loop.candidates->next )) {
			popListItem( &stack->loop.candidates );
			listItem *value = newItem( stack->loop.candidates->ptr );
			assign_variable( AssignSet, &variator_symbol, value, 0, context );
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
	return pop( state, event, next_state, context );
}

static int
pop_clause( char *state, int event, char **next_state, _context *context )
{
	StackVA *stack = (StackVA *) context->command.stack->ptr;
	if ( event == '~' ) {
		switch ( stack->clause.mode ) {
		case PassiveClause:
			context->clause.mode = ActiveClause;
			break;
		case ActiveClause:
		case PostActiveClause:
			context->clause.mode = PostActiveClause;
			break;
		case ClauseElse:
			return output( Error, "on '~': no condition to alternate" );
		default:
			return output( Error, "on '~': not in conditional execution mode" );
		}
		event = pop( state, event, next_state, context );
		*next_state = "?!:_/~";
	}
	else if ( stack->clause.mode == ClauseElse ) {
		event = pop( state, event, next_state, context );
	}
	else {
		context->clause.mode = stack->clause.mode;
		event = pop( state, event, next_state, context );
		*next_state = "?!:_/";
	}
	return event;
}
static int
pop_narrative( char *state, int event, char **next_state, _context *context )
{
	if (( context->command.level == context->record.level ) && ( context->record.instructions )) {
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
		else return outputf( Debug, "***** Error: pop_narrative: unexpected state '%s'", state );
	}
	event = pop( state, event, next_state, context );
	if ( context->command.level < context->record.level ) {
		// popping from narrative instruction block - return control to narrative or output
		*next_state = "";
	}
	return event;
}

/*---------------------------------------------------------------------------
	command_err
---------------------------------------------------------------------------*/
static int
command_err( char *state, int event, char **next_state, _context *context )
{
	// clear output target in case error happened in output state (>:)
	event = clear_output_target( state, event, next_state, context );

#if 0
	// in case we were reading HCN file, this will abort the answer
	// to CGI script if context->command.one is set, otherwise will
	// pop output target but keep parsing the HCN input
	context->input.hcn = 0;
#endif

	context->clause.mode = ClauseNone;
	context->clause.contrary = 0;
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
		int mode = input->mode;	// before pop
		event = pop_input( state, event, next_state, context );
		switch ( mode ) {
		case HCNFileInput:
		case HCNValueFileInput:
			context->output.cgi = 0;
			context->output.marked = 0;
			*next_state = ">:";
			break;
		case UNIXPipeInput:
		case FileInput:
			if ( context->command.one )
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
	else {
		output( Debug, "Error: unexpected command_pop_input()" );
		*next_state = "";
	}
	return event;
}

/*---------------------------------------------------------------------------
	command_read
---------------------------------------------------------------------------*/
int
command_read( void *src, InputType type, char *state, int event, _context *context )
{
	if ( event < 0 ) return event;

	int restore[ 3 ] = { 0, 0, 0 };
	restore[ 0 ] = context->input.level;
	restore[ 1 ] = context->command.one;
	restore[ 2 ] = context->command.block;

	context->command.one = ( type == InstructionOne );
	context->command.block = ( type == InstructionBlock );

	// push input
	// ---------------------------
	int retval = ((src) ? push_input( "", src, type, context ) : 0 );
	if ( retval < 0 ) return retval;

	// process input
	// ------------------------------
	do {
		if ( event || ( io_scan( state, event, &same, context ) >= 0 ) )
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
	push_input_hcn		- local
---------------------------------------------------------------------------*/
static int
push_input_hcn( char *state, int event, char **next_state, _context *context )
{
	char *scheme = context->identifier.scheme;
	char *path = context->identifier.path;
	if ( strcmp( scheme, "file" ) ) {
		return outputf( Error, ">:$< %s://%s ></> - operation not supported", scheme, path );
	}
        if ( !command_mode( 0, 0, ExecutionMode ) ) {
		*next_state = ">:";
                return 0;
	}
#ifdef DEBUG
	else if ( context->control.session )
		output( Debug, "PUSH_INPUT_HCN" );
#endif
	context->identifier.path = NULL;
	return push_input( "", path, HCNFileInput, context );
}

/*---------------------------------------------------------------------------
	check_out	- local
---------------------------------------------------------------------------*/
static int
check_out( char *state, int event, char **next_state, _context *context )
/*
	read_command enters the state 'out' upon each command completion,
	most notably upon '\n', but also in ">:" on '|'
	When this state is reached from a narrative single action command,
	then control must be returned to the narrative, unless the command
	consists of reading and outputing HCN contents - each line of which
	then translates into a single output command.
	The flag context->input.hcn is set when pushing HCN input stream,
	and unset when popping the HCN input stream.
*/
{
	if ( !strcmp( state, out ) ) {
		if ( context->input.hcn ) {
			*next_state = base;
		}
		else {
			event = clear_output_target( state, event, next_state, context );
			*next_state = ( context->command.one || (( event < 0 ) &&
				( context->narrative.mode.editing && (context->input.stack) ))) ?
				"" : base;
		}
	}
	else *next_state = same;

	return event;
}
