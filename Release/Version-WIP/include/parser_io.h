#ifndef PARSER_IO_H
#define PARSER_IO_H

#include "string_util.h"
#include "registry.h"

#define IO_ACTIVE	1
#define IO_SILENT	2
#define IO_ELSE		4
#define IO_ELIF		8
#define IO_UNPOUND	16

typedef enum {
	IOBuffer,
	IOStreamFile,
	IOStreamInput
} IOType;
typedef enum {
	IOErrNone = 1000,
	IOErrUnexpectedEOF,
	IOErrFileNotFound,
	IOErrIncludeFormat,
	IOErrTrailingChar,
	IOErrPragmaUnknown,
	IOErrPragmaIncomplete,
	IOErrAlreadyDefined,
	IOErrElseWithoutIf,
	IOErrEndifWithoutIf,
	IOErrElseAgain,
	IOErrUnterminatedIf,
	IOErrSyntaxError
} IOErr;
typedef enum {
	IOEventTake = 0,
	IOEventPragma,
	IOEventPass,
	IOEventQueue,
	IOEventRestore
} IOEventAction;
typedef enum {
	IOPragmaNone = 0,
	IOPragmaUnary,
	IOPragmaNary,
	IOPragma_include,
	IOPragma_define,
	IOPragma_undef,
	IOPragma_ifdef,
	IOPragma_ifndef,
	IOPragma_eldef,
	IOPragma_elndef,
	IOPragma_else,
	IOPragma_endif
} IOControlPragma;

typedef struct {
	void *	stream;
	char *	path;
	IOType	type;
	IOErr	errnum;
	int	line, column;
// private:
	char *	state;
	CNString *string;
	listItem *stack;
	listItem *buffer;
	struct {
		int mode;
		listItem *stack;
		IOControlPragma pragma;
		Registry *registry;
	} control;
} CNIO;

void	io_reset( CNIO *, int l, int c );
void	io_init( CNIO *, void *, char *, IOType, listItem * );
void	io_exit( CNIO * );
int	io_getc( CNIO *, int );
int	io_read( CNIO *, int );
IOErr	io_report( CNIO * );


#endif	// PARSER_IO_H
