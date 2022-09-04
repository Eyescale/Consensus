#ifndef NARRATIVE_PRIVATE_H
#define NARRATIVE_PRIVATE_H

// #define DEBUG

static CNOccurrence *newOccurrence( CNOccurrenceType );
static void freeOccurrence( CNOccurrence * );
static void occurrence_set( CNOccurrence *, listItem **sequence );
static void narrative_reorder( CNNarrative * );

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
	ErrMarkNoSub
} CNNarrativeError;

static void narrative_report( CNNarrativeError, int line, int column, int tabmark );

//===========================================================================
//	Narrative CNParserBegin / CNParserEnd - macros
//===========================================================================
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

#ifdef DEBUG
#define	DBGMonitor \
	fprintf( stderr, "readNarrative:l%dc%d: in \"%s\" on '%c'\n", line, column, state, event );
#else
#define	DBGMonitor
#endif

#define	CNParserBegin( parser ) \
	char *state = (parser)->state; \
	int column = (parser)->column; \
	int line = (parser)->line; \
	int event, caught, errnum=0; \
	do { \
		event = fgetc( (parser)->file ); \
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
