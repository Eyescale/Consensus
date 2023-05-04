#ifndef PARSER_IO_H
#define PARSER_IO_H

typedef enum {
	IOStreamFile
} IOStreamType;

typedef struct {
	IOStreamType type;
	void *	stream;
	char *	state;
	int	buffer,
		line, column;
} CNIO;

void	io_init( CNIO *, void *, IOStreamType );
void	io_exit( CNIO * );
int	io_getc( CNIO *, int );


#endif	// PARSER_IO_H
