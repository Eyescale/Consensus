#ifndef PARSER_H
#define PARSER_H

#include "macros.h"
#include "database.h"
#include "story.h"

typedef struct {
	FILE *	stream;
	char *	state;
	int	line, column,
		mode[ 4 ], buffer,
		errnum;
	void *	user_data;
} CNParserData;

typedef struct {
// shared between parser and story
	CNString *	string;
	int		tab[4], type, flags;
	listItem *	stack; 
// story only
	CNDB *		db;
	CNStory *	story;
	CNNarrative *	narrative;
	Pair *		entry;
	CNOccurrence *	occurrence;
} BMStoryData;

// tab data informed by bm_parse

#define TAB_CURRENT	tab[0]
#define TAB_LAST	tab[1]
#define TAB_SHIFT	tab[2]
#define TAB_BASE	tab[3]

// flag values handled by bm_parse

#define FIRST		1
#define FILTERED	2
#define SUB_EXPR	4
#define MARKED		8
#define NEGATED		16
#define INFORMED	32
#define LEVEL		64
#define SET		128
#define ASSIGN		256
#define DOT		512
#define COMPOUND	1024

typedef enum {
	BM_INI = 1,
	BM_STORY,
	BM_INSTANCE
} BMParseMode;
typedef enum {
	isBaseNarrative = 1,
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

typedef int (*BMParseCB)( BMParseOp, BMParseMode, void * );
char *	bm_parse( int event, CNParserData *, BMParseMode, BMParseCB );
void	bm_parser_report( BMParserError, CNParserData *, BMParseMode );

int	cn_parser_init( CNParserData *, char *state, FILE *, void * );
int	cn_parser_getc( CNParserData * );

#endif	// PARSER_H
