#ifndef COMMAND_UTIL_H
#define COMMAND_UTIL_H

/*---------------------------------------------------------------------------
	actions		- public
---------------------------------------------------------------------------*/

_action build_expression;
_action dereference_expression;
_action assimilate_expression;
_action evaluate_expression;
_action test_expression;
_action solve_expression;
_action expression_op;
_action set_command_op;
_action set_results_to_nil;

_action narrative_op;
_action exit_command;
_action input_inline;
_action input_story;
_action output_story;
_action set_output_target;
_action backfeed_output;
_action sync_delivery;
_action sync_output;
_action pipe_output_in;
_action pipe_output_out;
_action pipe_output_to_variable;
_action clear_output_target;
_action set_assignment_mode;
_action scheme_op;
_action handle_iam;
_action handle_bye;
_action input_cgi;

/*---------------------------------------------------------------------------
	utilities	- public
---------------------------------------------------------------------------*/


#endif	// COMMAND_UTIL_H
