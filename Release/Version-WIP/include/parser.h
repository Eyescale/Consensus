#ifndef PARSER_H
#define PARSER_H

#include "macros.h"
#include "story.h"

typedef struct {
	FILE *	stream;
	char *	state;
	int	line, column,
		mode[ 4 ], buffer,
		errnum;
	void *	user_data;
}
BMParserData;

int	cnParserGetc( BMParserData * );

// #define DEBUG

//===========================================================================
//	BM Parser
//===========================================================================
typedef enum {
	ErrNone = 0,
	ErrUnknownState,
	ErrUnexpectedEOF,
	ErrSpace,
	ErrUnexpectedCR,
	ErrIndentation,
	ErrSyntaxError,
	ErrExpressionSyntaxError,
	ErrInputScheme,
	ErrOutputScheme,
	ErrMarkMultiple,
	ErrMarkNegated,
	ErrNarrativeEmpty,
	ErrNarrativeDouble,
	ErrUnknownCommand,
} BMParserError;

int	bm_parser_init( BMParserData *, char *, FILE *, void * );
char *	bm_parse( int event, BMParserData *, BMReadMode );
void	bm_parser_report( BMParserError, BMParserData *, BMReadMode );


#endif	// PARSER_H
