#ifndef COMMAND_UTIL_H
#define COMMAND_UTIL_H

/*---------------------------------------------------------------------------
	actions		- public
---------------------------------------------------------------------------*/

_action set_results_to_nil;
_action set_expression_mode;
_action expression_op;
_action evaluate_expression;
_action narrative_op;
_action exit_command;
_action parse_expression;
_action input_inline;
_action input_story;
_action output_story;
_action set_output_from_variable;
_action set_output_target;
_action backfeed_output;
_action sync_delivery;
_action sync_output;
_action pipe_output_in;
_action pipe_output_out;
_action clear_output_target;
_action set_assignment_mode;
_action open_stream;
_action scheme_op;
_action scheme_op_pipe;
_action handle_iam;
_action cgi_entry_bgn;
_action cgi_entry_add;
_action cgi_entry_end;
_action read_cgi_entry;
_action set_cgi_entry;

/*---------------------------------------------------------------------------
	utilities	- public
---------------------------------------------------------------------------*/

void set_command_mode( CommandMode mode, _context *context );
int command_mode( int freeze, int instruct, int execute );


#endif	// COMMAND_UTIL_H
