#ifndef IO_H
#define IO_H

typedef enum {
	IOStreamFile
} IOStreamType;

typedef struct {
	IOStreamType type;
	void *	stream;
	int	line, column,
		mode[ 4 ], buffer;
} CNIO;

void	io_init( CNIO *, void *, IOStreamType );
void	io_exit( CNIO * );
int	io_getc( CNIO *, int );


#endif	// IO_H
