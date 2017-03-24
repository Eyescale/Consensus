#ifndef VARIABLES_H
#define VARIABLES_H

/*---------------------------------------------------------------------------
	variables actions		- public
---------------------------------------------------------------------------*/

_action	reset_variables;
_action	variable_reset;
_action	assign_results;
_action	assign_narrative;
_action	assign_expression;
_action	assign_va;

/*---------------------------------------------------------------------------
	variables utilities	- public
---------------------------------------------------------------------------*/

registryEntry *lookupVariable( _context *context, char *identifier );

void	set_this_variable( Registry *variables, Entity *this_value );
void	freeVariableValue( VariableVA *variable );
void	freeVariables( registryEntry **variables );
void	assign_variator_variable( Entity *index, int type, _context *context );


#endif	// VARIABLES_H
