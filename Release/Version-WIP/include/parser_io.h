#ifndef PARSER_IO_H
#define PARSER_IO_H

#include "string_util.h"
#include "registry.h"

#define IO_ACTIVE	1
#define IO_SILENT	2
#define IO_ELSE		4
#define IO_ACTIVE_ELSE	(IO_ACTIVE|IO_ELSE)
#define IO_SILENT_ELSE	(IO_SILENT|IO_ELSE)

typedef enum {
	IOBuffer,
	IOStreamFile,
	IOStreamStdin
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
	IOErrIfUnterminated,
	IOErrSyntaxError
} IOErr;
typedef enum {
	IOEventTake = 0,
	IOEventPragma,
	IOEventPass,
	IOEventQueue,
	IOEventFlush
} IOEventAction;
typedef enum {
	IOPragmaNone = 0,
	IOIncludeFile,
	IODefine,
	IOUndef,
	IOIfdef,
	IOIfndef,
	IOElse,
	IOEndif
} IOControlPragma;

typedef struct {
	CNString *string;
	listItem *stack;
	listItem *buffer;
	char *	state;
	IOType	type;
	void *	stream;
	char *	path;
	int	line, column;
	IOErr	errnum;
	struct {
		int mode;
		listItem *stack;
		IOControlPragma pragma;
		void *param;
		Registry *registry;
	} control;
} CNIO;

void	io_init( CNIO *, void *, IOType );
void	io_exit( CNIO * );
int	io_getc( CNIO *, int );
IOErr	io_report( CNIO * );


#endif	// PARSER_IO_H
