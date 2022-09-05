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
CNParserData;

int	cnParserGetc( CNParserData * );

// #define DEBUG

//===========================================================================
//	BM Parser
//===========================================================================
typedef struct {
	CNStory *	story;
	CNNarrative *	narrative;
	CNOccurrence *	occurrence;
	CNString *	string;
	int		tab[4], type, flags;
	listItem *	stack; 
} BMStoryData;

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

int	bm_parser_init( CNParserData *, BMStoryData *, FILE *, BMReadMode );
char *	bm_parse( int event, CNParserData *, BMReadMode );
void	bm_parser_report( BMParserError, CNParserData *, BMReadMode );
void *	bm_parser_exit( CNParserData *, BMReadMode );


#endif	// PARSER_H
