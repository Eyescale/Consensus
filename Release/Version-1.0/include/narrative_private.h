#ifndef NARRATIVE_PRIVATE_H
#define NARRATIVE_PRIVATE_H

typedef listItem Sequence;

static CNOccurrence *newOccurrence( CNOccurrenceType );
static void freeOccurrence( CNOccurrence * );
static void occurrence_set( CNOccurrence *, Sequence **sequence );
static int narrative_build( listItem **stack, int type, int typelse, int *prev, int tabs );
static void narrative_reorder( CNNarrative * );

static void push_position( listItem **stack, int first );
static void add_item( Sequence **Sequence, int event );
static void freeSequence( Sequence ** );
static void push_sub( listItem * );
static void pop_sub( listItem ** );
static int tag_sub( listItem * );

typedef enum {
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

#define	CNParserBegin( file ) \
	char *state = "base"; \
	int event, line=1, column=0, errnum=0, reenter; \
	do { \
		event = fgetc( file ); column++; \
		do { \
			reenter = 0; \
			DBGMonitor \
			bgn_
#define CNParserEnd \
			end \
			if ( errnum ) reenter = 1; \
		} while ( reenter ); \
		if ( event=='\n' ) { line++; column=0; } \
	} while ( strcmp( state, "" ) );


#endif	// NARRATIVE_PRIVATE_H
