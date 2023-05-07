#ifndef PARSER_IO_H
#define PARSER_IO_H

#include "string_util.h"

typedef enum {
	IOStreamFile,
	IOStreamStdin
} IOType;
typedef enum {
	IOErrNone = 0,
	IOErrFileNotFound,
	IOErrIncludeFormat,
	IOErrTrailingChar,
	IOErrSyntaxError
} IOErr;

typedef struct {
	CNString *string;
	listItem *stack;
	listItem *buffer;
	char *	state;
	IOType	type;
	void *	stream;
	char *	path;
	int	line, column;
} CNIO;

void	io_init( CNIO *, void *, IOType );
void	io_exit( CNIO * );
int	io_getc( CNIO *, int );


#endif	// PARSER_IO_H
