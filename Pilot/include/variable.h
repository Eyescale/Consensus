#ifndef VARIABLE_H
#define VARIABLE_H

/*---------------------------------------------------------------------------
	variables actions		- public
---------------------------------------------------------------------------*/

_action	reset_variables;
_action	reset_variable;
_action	assign_results;
_action	assign_literals;
_action	assign_va;
_action	assign_narrative;
_action	assign_expression;

/*---------------------------------------------------------------------------
	variables utilities	- public
---------------------------------------------------------------------------*/

void	assign_this_variable( Registry *variables, Entity *this_value );
void	assign_occurrence_variable( char *identifier, void *value, ValueType, _context * );
int	assign_variable( AssignmentMode, char **identifier, void *value, ValueType, _context * );


#endif	// VARIABLE_H
