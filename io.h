#ifndef IO_H
#define IO_H

typedef enum {
	SessionConnection = 1,
	ServiceRequest
}
ConnectionType;

/*---------------------------------------------------------------------------
	io actions	- public
---------------------------------------------------------------------------*/

_action	io_scan;

/*---------------------------------------------------------------------------
	io utilities	- public
---------------------------------------------------------------------------*/

int	io_init( _context *context );
int	io_exit( _context *context );
int	io_connect( ConnectionType type, char *path );
int	io_read( int fd, char *buffer, int *remainder );
int	io_write( int fd, char *string, int length, int *remainder );
int	io_flush( int fd, int *remainder );

#endif	// IO_H
