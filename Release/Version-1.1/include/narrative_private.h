#ifndef NARRATIVE_PRIVATE_H
#define NARRATIVE_PRIVATE_H

#include "macros.h"

// #define DEBUG

typedef enum {
	ErrUnknownState,
	ErrUnexpectedEOF,
	ErrSpace,
	ErrUnexpectedCR,
	ErrIndentation,
	ErrSyntaxError,
	ErrSequenceSyntaxError,
	ErrInputScheme,
	ErrOutputScheme,
	ErrMarkMultiple,
	ErrMarkNoSub,
	ErrProtoRedundantArg
} CNNarrativeError;
static void err_report( CNNarrativeError, int line, int column );

typedef struct {
	FILE *	file;
	char *	state;
	int	caught,
		line,
		column,
		mode[ 4 ],
		buffer;
	void *	user_data;
}
CNParserData;
static int parser_getc( CNParserData * );

static int add_narrative( listItem **story, CNNarrative *);

static CNNarrative * newNarrative( void );
static void freeNarrative( CNNarrative * );
static int proto_set( CNNarrative *, CNString * );
static void narrative_reorder( CNNarrative * );
static int narrative_output( FILE *, CNNarrative *, int );

static CNOccurrence *newOccurrence( CNOccurrenceType );
static void freeOccurrence( CNOccurrence * );
static void occurrence_set( CNOccurrence *, CNString *sequence );

static void dirty_set( int *dirty, CNString *s );
static void dirty_go( int *dirty, CNString *s );
static void s_add( CNString *, char * );

//===========================================================================
//	Narrative CNParserBegin / CNParserEnd - macros
//===========================================================================
#ifdef DEBUG
#define	DBGMonitor \
	fprintf( stderr, "CNParser:l%dc%d: in \"%s\" on '%c'\n", line, column, state, event );
#else
#define	DBGMonitor
#endif

#define	CNParserBegin( parser ) \
	char *state = (parser)->state; \
	int column = (parser)->column; \
	int line = (parser)->line; \
	int event, caught, errnum=0; \
	do { \
		event = parser_getc( parser ); \
		if ( event != EOF ) column++; \
		do { \
			caught = CNCaughtEvent; \
			DBGMonitor; bgn_
#define CNParserDefault \
			end \
			if ( caught & CNCaughtTrans ) ; \
			else bgn_
#define CNParserEnd \
			end \
		} while ( !(caught & CNCaughtEvent) ); \
		if ( event=='\n' ) { line++; column=0; } \
	} while ( strcmp( state, "" ) );


#endif	// NARRATIVE_PRIVATE_H
