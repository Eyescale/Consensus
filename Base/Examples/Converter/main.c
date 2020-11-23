#include <stdio.h>
#include <stdlib.h>

#include "list.h"
#include "parser.h"

extern void translate( Parser *, Sequence * );

static void
clarg( int argc, char *argv[], int *tokenized )
{
	char *path;
	*tokenized = 1;
	switch ( argc ) {
	case 1: break;
	case 2: if ( !strcmp( argv[1], "-n" ) ) {
			*tokenized = 0;
			break;
		 }
		 // no break
	default:
		fprintf( stderr, "Usage: translate [-n]\n" );
		exit( -1 );
	}
}

/*---------------------------------------------------------------------------
	main
---------------------------------------------------------------------------*/
typedef struct {
	int current;
	listItem *buffer;
} Status;
static int check( Status *, Parser *, int p_stat, int event );
static int NextState( int, int );
enum {
	NewLine,
	Slash,
	Flush,
	InLine,
	Exit
};

int
main( int argc, char *argv[] )
{
	int tokenized;
	clarg( argc, argv, &tokenized );
	int p_opt = tokenized ? P_TOK : 0;
	Scheme *scheme = readScheme( "./scheme.y" );
	Parser *parser = newParser( scheme, p_opt );
	if ( parser == NULL ) {
		fprintf( stderr, "yak$ Error: parser allocation failed\n" );
		freeScheme( scheme );
		exit( -1 );
	}
	outputScheme( scheme, tokenized );
	static Status status = { 0, NULL };
	int line = 0, column;
	int event='\n', state = NewLine;
	do {
		if ( event == '\n') {
			line++; column = 1;
			fprintf( stderr, "yak%d$ ", line );
		}
		else column++;
		event = getchar();
		if ( state != Flush ) {
			switch ( ParserFrame( parser, event ) ) {
				case rvParserOutOfMemory:
					fprintf( stderr, "Error: parser out of memory\n" );
					state = Exit;
					break;
				case rvParserNoMatch:
					/* here all parser rules failed
					*/
					if ( check( &status, parser, rvParserNoMatch, event ) ) {
						fprintf( stderr, "Error: l%dc%d: ", line, column );
						if ( event == '\n' ) {
							fprintf( stderr, "statement incomplete\n" );
						} else {
							fprintf( stderr, "syntax error\n" );
							state = Flush;
						}
					}
					ParserRebase( parser, P_ALL );
					break;
				case rvParserEncore:
					/* Here there are still active parser base threads.
					   We choose to ignore all intermediate results, and
					   keep threads deactivated until the longest one succeeds
					   (or fails).
					*/
					check( &status, parser, rvParserEncore, event );
					ParserRebase( parser, P_RESULTS );
					break;
				case rvParserNoMore:
					/* here all parser rules completed and at least one
					   base rules succeeded. It is up to the implementation
					   to call it an error or not
					*/
					; int p_stat = check( &status, parser, rvParserNoMore, event );
					if ( p_stat > 0 ) {
						for ( listItem *i=parser->results; i!=NULL; i=i->next )
							translate( parser, i->ptr );
						printf( "\n" );
					}
					else if ( p_stat < 0 ) {
						fprintf( stderr, "Warning: l%dc%d: ", line, column );
						fprintf( stderr, "standalone CN start statement\n" );
					}
					ParserRebase( parser, P_ALL );
					break;
			}
		}
		state = NextState( state, event );
	}
	while ( state != Exit );
	freeParser( parser );
	freeScheme( scheme );
}

static int
NextState( int state, int event )
{
	switch ( state ) {
		case NewLine: switch ( event ) {
			case EOF : state = Exit; break;
			case '\n': break;
			case '/' : state = Slash; break;
			default  : state = InLine;
			}
			break;
		case Slash: switch ( event ) {
			case EOF : state = Exit; break;
			case '\n': state = Exit; break;
			default  : state = InLine;
			}
			break;
		case Flush:
		case InLine: switch ( event ) {
			case EOF : state = Exit; break;
			case '\n': state = NewLine; break;
			}
			break;
		}
	return state;
}

/*---------------------------------------------------------------------------
	check
---------------------------------------------------------------------------*/
static void buffer_flush( listItem **buffer, int event );
enum {
	PARSER_READY = 0,
	PARSER_FREEZE,
	PARSER_STEADY,
	PARSER_GO
};
static int
check( Status *status, Parser *parser, int p_stat, int event )
/*
   returns
	-1 when standalone START sequence has been found
	+1 when processing past START sequence
	0 otherwise
*/
{
	union { int value; void *ptr; } icast;

	switch ( status->current ) {
	case PARSER_READY:
		switch ( p_stat ) {
		case rvParserEncore:
			buffer_flush( &status->buffer, 0 );
			icast.value = event;
			addItem( &status->buffer, icast.ptr );
			status->current = PARSER_STEADY;
			break;
		case rvParserNoMore:
		case rvParserNoMatch:
			printf( "%c", event );
			if ( event != '\n' )
				status->current = PARSER_FREEZE;
			break;
		}
		break;
	case PARSER_FREEZE:
		printf( "%c", event );
		if ( event == '\n' )
			status->current = PARSER_READY;
		break;
	case PARSER_STEADY:
		switch ( p_stat ) {
		case rvParserEncore:
			/* parser is alive & kicking
			*/
			if (( parser->results )) {
				/* we assume the sequence here is START
				*/
				freeListItem( &status->buffer );
				status->current = PARSER_GO;
			}
			else {
				icast.value = event;
				addItem( &status->buffer, icast.ptr );
			}
			break;
		case rvParserNoMore:
			/* standalone start sequence or exit
			*/
			freeListItem( &status->buffer );
			status->current = PARSER_READY;
			if (( parser->results )) {
				if ( parser->options & P_TOK ) {
					/* we assume the sequence here is START
					*/
					return -1;
				}
				/* parser->results is a list of sequences, where
				   each sequence is a list of pairs (cf. translate.c)
				*/
				listItem *sequence = parser->results->ptr;
				Pair *pair = sequence->ptr;
				if ((int) pair->value != '/' )
					return -1;
			}
			break;
		case rvParserNoMatch:
			/* here we never got the START sequence
			*/
			buffer_flush( &status->buffer, event );
			status->current = PARSER_READY;
			break;
		}
		break;
	case PARSER_GO:
		switch ( p_stat ) {
		case rvParserEncore:
			break;
		case rvParserNoMore:
		case rvParserNoMatch:
			status->current = PARSER_READY;
		}
		return 1;
	}
	return ( status->current == PARSER_GO );
}
static void
buffer_flush( listItem **buffer, int event )
{
	reorderListItem( buffer );
	for ( listItem *i=*buffer; i!=NULL; i=i->next )
		printf( "%c", (int) i->ptr );
	freeListItem( buffer );
	if ( event ) printf( "%c", event );
}

