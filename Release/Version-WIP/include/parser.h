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
	void *	user_data;
} CNParser;

int	cn_parser_init( CNParser *, char *state, FILE *, void * );
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
} CNParseStory;

// tab data informed by bm_parse

#define TAB_CURRENT	tab[0]
#define TAB_LAST	tab[1]
#define TAB_SHIFT	tab[2]
#define TAB_BASE	tab[3]

typedef enum {
	CN_LOAD = 1,
	CN_INPUT,
	CN_STORY,
} CNParseMode;
typedef enum {
	NarrativeTake = 1,
	ProtoSet,
	OccurrenceAdd,
	ExpressionPush,
	ExpressionPop,
	ExpressionTake
} CNParseOp;
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
} CNParseErr;

typedef int (*CNParseCB)( CNParseOp, CNParseMode, void * );
char *	cn_parse( int event, CNParser *, CNParseMode, CNParseCB );
int	cn_parse_init( CNParseStory *, CNParser *, CNParseMode, char *state, FILE * );
void	cn_parse_report( CNParseErr, CNParser *, CNParseMode );

#endif	// PARSER_H
