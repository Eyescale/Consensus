#ifndef PARSER_H
#define PARSER_H

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
	int		tab[4], type;
	listItem *	stack; 
// story only
	CNDB *		db;
	CNStory *	story;
	CNNarrative *	narrative;
	Pair *		entry;
	CNOccurrence *	occurrence;
// parser only
	int		flags, opt;
} BMStoryData;

// tab data informed by bm_parse

#define TAB_CURRENT	tab[0]
#define TAB_LAST	tab[1]
#define TAB_SHIFT	tab[2]
#define TAB_BASE	tab[3]

typedef enum {
	BM_INI = 1,
	BM_STORY,
	BM_INSTANCE
} BMParseMode;
typedef enum {
	NarrativeTake = 1,
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
int	bm_parser_init( CNParserData *, BMParseMode );
void	bm_parser_report( BMParserError, CNParserData *, BMParseMode );

int	cn_parser_init( CNParserData *, char *state, FILE *, void * );
int	cn_parser_getc( CNParserData * );

#endif	// PARSER_H
