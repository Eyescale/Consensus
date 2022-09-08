#ifndef PARSER_H
#define PARSER_H

#include "macros.h"

//===========================================================================
//	Parser - Story Interface
//===========================================================================
typedef struct {
	FILE *	stream;
	char *	state;
	int	line, column,
		mode[ 4 ], buffer,
		errnum;
	void *	user_data;
} CNParserData;
typedef enum {
	BM_INI = 1,
	BM_STORY,
	BM_INSTANCE
} BMParseMode;
typedef enum {
	isNarrativeBase = 1,
	NarrativeTake,
	ProtoSet,
	OccurrenceAdd,
	ExpressionPush,
	ExpressionPop,
	ExpressionTake
} BMParseOp;
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

int	cn_parser_init( CNParserData *, char *state, FILE *, void * );
int	cn_parser_getc( CNParserData * );

typedef int (*BMParseCB)( BMParseOp, BMParseMode, void * );
char *	bm_parse( int event, CNParserData *, BMParseMode, BMParseCB );
void	bm_parser_report( BMParserError, CNParserData *, BMParseMode );


#endif	// PARSER_H
