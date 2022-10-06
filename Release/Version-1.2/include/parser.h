#ifndef PARSER_H
#define PARSER_H

#include "context.h"
#include "narrative.h"
#include "string_util.h"

typedef struct {
	FILE *	stream;
	char *	state;
	int	line, column,
		mode[ 4 ], buffer,
		errnum;
} CNParser;

int	cn_parser_init( CNParser *, char *state, FILE * );
int	cn_parser_getc( CNParser * );

//===========================================================================
//	bm_read - bm_parse interface
//===========================================================================
typedef struct {
// shared between parser and bm_read()
	struct {
		listItem *flags; // bm_parse() only
		listItem *occurrences; // bm_read() only
	} stack; 
	CNParser *	parser;
	CNString *	string;
	int		tab[4], type;
// bm_read() only
	BMContext *	ctx;
	CNStory *	story;
	CNNarrative *	narrative;
	Pair *		entry;
	CNOccurrence *	occurrence;
// bm_parse() only
	int		flags, opt;
} BMParseData;

// tab data informed by bm_parse

#define TAB_CURRENT	tab[0]
#define TAB_LAST	tab[1]
#define TAB_SHIFT	tab[2]
#define TAB_BASE	tab[3]

typedef BMReadMode BMParseMode;
typedef enum {
	NarrativeTake = 1,
	ProtoSet,
	OccurrenceAdd,
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
} BMParseErr;

typedef int (*BMParseCB)( BMParseOp, BMParseMode, void * );
char *	bm_parse( int event, BMParseData *, BMParseMode, BMParseCB );
int	bm_parse_init( BMParseData *, CNParser *, BMParseMode, char *state, FILE * );
void	bm_parse_report( BMParseData *, BMParseErr, BMParseMode );

#endif	// PARSER_H
