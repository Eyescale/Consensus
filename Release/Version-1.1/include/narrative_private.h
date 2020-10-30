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
#ifdef DEBUG
#define	DBGMonitor \
	fprintf( stderr, "readNarrative:l%dc%d: in \"%s\" on '%c'\n", line, column, state, event );
#else
#define	DBGMonitor
#endif

#define	CNParserBegin( file, input ) \
	char *state = "base"; \
	int event, line=1, column=0, errnum=0; \
	struct { int event, state, transition; } caught; \
	int mode[4] = { 0, 0, 0, 0 }, buffer = 0; \
	do { \
		event = input( file, mode, &buffer, &column, &line ); \
		if ( event != EOF ) column++; \
		do { \
			caught.transition = 0; \
			caught.state = 0; \
			caught.event = 1; \
			DBGMonitor \
			bgn_
#define CNParserDefault \
			end \
			if ( caught.transition ) ; \
			else bgn_
#define CNParserEnd \
			end \
		} while ( !caught.event ); \
		if ( event=='\n' ) { line++; column=0; } \
	} while ( strcmp( state, "" ) );


#endif	// NARRATIVE_PRIVATE_H
