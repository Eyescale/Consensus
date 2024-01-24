#ifndef PARSER_H
#define PARSER_H

#include "context.h"
#include "narrative.h"
#include "story.h"
#include "string_util.h"
#include "parser_io.h"

//===========================================================================
//	bm_read - bm_parse interface
//===========================================================================
typedef enum {
	BM_LOAD = 1,
	BM_INPUT,
	BM_STORY,
	BM_CMD
} BMParseMode;

typedef struct {
// shared between parser and bm_read()
	struct {
		listItem *flags; // bm_parse() only
		listItem *occurrences; // bm_read() only
	} stack; 
	CNIO *		io;
	CNString *	string;
	int		tab[4], type;
// bm_read() only
	BMContext *	ctx;
	CNOccurrence *	occurrence;
	CNNarrative *	narrative;
	Registry *	narratives;
	Pair *		entry;
// bm_parse() only
	char *		state;
	int		errnum;
	int		flags;
	int		expr;
} BMParseData;

// tab data informed by bm_parse

#define TAB_CURRENT	tab[0]
#define TAB_LAST	tab[1]
#define TAB_SHIFT	tab[2]
#define TAB_BASE	tab[3]

typedef enum {
	NarrativeTake = 1,
	ProtoSet,
	OccurrenceAdd,
	ExpressionTake
} BMParseOp;
typedef enum {
	ErrNone = 0,
	WarnOutputFormat,
	WarnInputFormat,
	WarnUnsubscribe,
	WarnNeverLoop,
	ErrUnknownState,
	ErrUnexpectedEOF,
	ErrUnexpectedCR,
	ErrUnexpectedSpace,
	ErrNarrativeEmpty,
	ErrNarrativeNoEntry,
	ErrNarrativeDoubleDef,
	ErrEllipsisLevel,
	ErrIndentation,
	ErrSyntaxError,
	ErrExpressionSyntaxError,
	ErrInputScheme,
	ErrOutputScheme,
	ErrMarkDo,
	ErrMarkOn,
	ErrMarkGuard,
	ErrMarkTernary,
	ErrMarkMultiple,
	ErrMarkNegated,
	ErrEMarked,
	ErrEMarkMultiple,
	ErrEMarkNegated,
	ErrPerContrary,
	ErrUnknownCommand,
} BMParseErr;

typedef int (*BMParseCB)( BMParseOp, BMParseMode, void * );
typedef char * (*BMParseFunc)( int event, BMParseMode, BMParseData *, BMParseCB );

char * bm_parse_cmd( int event, BMParseMode, BMParseData *, BMParseCB );
char * bm_parse_expr( int event, BMParseMode, BMParseData *, BMParseCB );
char * bm_parse_load( int event, BMParseMode, BMParseData *, BMParseCB );
char * bm_parse_ui( int event, BMParseMode, BMParseData *, BMParseCB );

void	bm_parse_init( BMParseData *, BMParseMode mode );
void	bm_parse_exit( BMParseData * );
void	bm_parse_caution( BMParseData *, BMParseErr, BMParseMode );
void	bm_parse_report( BMParseData *, BMParseMode, int, int );

#endif	// PARSER_H
