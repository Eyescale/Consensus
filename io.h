#ifndef IO_H
#define IO_H

/*---------------------------------------------------------------------------
	ipc actions	- public
---------------------------------------------------------------------------*/

_action	io_read;

/*---------------------------------------------------------------------------
	ipc utilities	- public
---------------------------------------------------------------------------*/

int	io_write( _context *context, OutputType type, const char *format, va_list ap );
int	io_open( _context *context );
int	io_close( _context *context );
void	io_save_changes( _context *context );

#endif	// IO_H
