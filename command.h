#ifndef COMMAND_H
#define COMMAND_H

/*---------------------------------------------------------------------------
	command actions		- public
---------------------------------------------------------------------------*/

_action read_command;


/*---------------------------------------------------------------------------
	command utilities	- public
---------------------------------------------------------------------------*/

int	command_read( void *src, InputType type, char *state, int event, _context *context );


#endif	// COMMAND_H
