#ifndef IO_H
#define IO_H

#define IO_PORT	1
#define IO_CALL	2

/*---------------------------------------------------------------------------
	io actions	- public
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
	io utilities	- public
---------------------------------------------------------------------------*/

int	io_open( int type, char *format, ... );
void	io_close( int type, int socket_fd, char *format, ... );
int	io_init( _context *context );
int	io_exit( _context *context );
void	io_scan( fd_set *fds, int block, _context *context );
void	io_inform( _context *context );
int	io_accept( int fd, _context *context );
int	io_call( char *path, _context *context );
int	io_read( int fd, char *buffer, int *remainder );
int	io_write( int fd, char *string, int length, int *remainder );
int	io_flush( int fd, int *remainder, _context *context );

#endif	// IO_H
