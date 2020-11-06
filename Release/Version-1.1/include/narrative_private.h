#ifndef NARRATIVE_PRIVATE_H
#define NARRATIVE_PRIVATE_H

// #define DEBUG

static int input( FILE *file, int *mode, int *buffer, int *c, int *l );
static int preprocess( int event, int *mode, int *buffer, int *skipped );

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
	ErrProtoSyntaxError,
	ErrProtoRedundantArg
} CNNarrativeError;

static void err_report( CNNarrativeError, int line, int column, int tabmark );

static CNNarrative * newNarrative( void );
static void freeNarrative( CNNarrative * );
static void narrative_reorder( CNNarrative * );
static int narrative_output( FILE *, CNNarrative *, int );

static CNOccurrence *newOccurrence( CNOccurrenceType );
static void freeOccurrence( CNOccurrence * );
static void occurrence_set( CNOccurrence *, listItem **sequence );

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
