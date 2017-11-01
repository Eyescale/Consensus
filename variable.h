#ifndef VARIABLE_H
#define VARIABLE_H

/*---------------------------------------------------------------------------
	variables actions		- public
---------------------------------------------------------------------------*/

_action	variable_reset;
_action	reset_variables;
_action	assign_results;
_action	assign_va;
_action	assign_narrative;
_action	assign_expression;
_action	read_variable_id;
_action	read_variable_ref;

/*---------------------------------------------------------------------------
	variables utilities	- public
---------------------------------------------------------------------------*/

void	set_this_variable( Registry *variables, Entity *this_value );
void	assign_variable( char **identifier, void *value, int type, _context *context );


#endif	// VARIABLE_H
