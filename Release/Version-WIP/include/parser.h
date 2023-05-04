#ifndef PARSER_H
#define PARSER_H

#include "context.h"
#include "narrative.h"
#include "string_util.h"
#include "parser_io.h"

//===========================================================================
//	bm_read - bm_parse interface
//===========================================================================
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
	CNStory *	story;
	CNNarrative *	narrative;
	Pair *		entry;
	CNOccurrence *	occurrence;
// bm_parse() only
	char *		state;
	int		errnum;
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
	WarnOutputFormat,
	WarnInputFormat,
	ErrUnknownState,
	ErrUnexpectedEOF,
	ErrEllipsisLevel,
	ErrSpace,
	ErrInstantiationFiltered,
	ErrUnexpectedCR,
	ErrIndentation,
	ErrSyntaxError,
	ErrExpressionSyntaxError,
	ErrInputScheme,
	ErrOutputScheme,
	ErrMarkMultiple,
	ErrMarkNegated,
	ErrEMarked,
	ErrEMarkMultiple,
	ErrEMarkNegated,
	ErrPerContrary,
	ErrUnknownCommand,
} BMParseErr;

typedef int (*BMParseCB)( BMParseOp, BMParseMode, void * );
char *	bm_parse( int event, BMParseMode, BMParseData *, BMParseCB );
void	bm_parse_init( BMParseData *, BMParseMode mode );
void	bm_parse_exit( BMParseData * );
void	bm_parse_caution( BMParseData *, BMParseErr, BMParseMode );
void	bm_parse_report( BMParseData *, BMParseMode );

#endif	// PARSER_H
