#ifndef COMMAND_H
#define COMMAND_H

/*---------------------------------------------------------------------------
	command actions		- public
---------------------------------------------------------------------------*/

_action read_command;


/*---------------------------------------------------------------------------
	command utilities	- public
---------------------------------------------------------------------------*/

int	command_init( void *src, InputType type, int *restore, _context *context );
int	command_exit( char *state, int event, int *restore, _context *context );


#endif	// COMMAND_H
