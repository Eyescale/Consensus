#ifndef IO_H
#define IO_H

enum {
	IO_TICKET = 1,
	IO_QUERY,
	IO_PORT
};

/*---------------------------------------------------------------------------
	io actions	- public
---------------------------------------------------------------------------*/

_action	io_scan;

/*---------------------------------------------------------------------------
	io utilities	- public
---------------------------------------------------------------------------*/

int	io_open( int type, void *fmt, ... );
void	io_close( int type, int socket_fd, char *format, ... );
int	io_init( _context *context );
int	io_exit( _context *context );
void	io_notify( FrameLog *log, _context *context );
int	io_query( char *path, _context *context );
int	io_read( int fd, char *buffer, int *remainder );
int	io_write( int fd, char *string, int length, int *remainder );
int	io_flush( int fd, int *remainder, int eot, _context *context );

#endif	// IO_H
