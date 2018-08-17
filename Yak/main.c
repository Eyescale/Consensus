#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "list.h"
#include "parser.h"

static char *
clarg( int argc, char *argv[], int *token, int *space )
{
	char *path;
	*token = 1;
	*space = 0;
	switch ( argc ) {
	case 2:
		path = argv[ 1 ];
		break;
	case 3:
		path = argv[ 2 ];
		char *o = argv[ 1 ];
		if ( *o++ == '-' ) {
			for ( ; *o; o++ ) {
				switch ( *o ) {
				case 'n': *token = 0; break;
				case 's': *space = 1; break;
				}
			}
			break;
		}
		// no break
	default:
		/* option -n is for 'no token'
		   option -s is for 'space' (recorded)
		*/
		fprintf( stderr, "Usage: yak [-ns] path\n" );
		exit( -1 );
	}
	return path;
}

/*---------------------------------------------------------------------------
	main
---------------------------------------------------------------------------*/
static void outputSequence( Parser *, Sequence * );
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
	int token, space;
	char *path = clarg( argc, argv, &token, &space );
	int p_opt = ( token ? P_TOK : 0 ) | ( space ? P_SPACE : 0 );
	Scheme *scheme = readScheme( path );
	Parser *parser = newParser( scheme, p_opt );
	if ( parser == NULL ) {
		fprintf( stderr, "yak$ Error: parser allocation failed\n" );
		freeScheme( scheme );
		exit( -1 );
	}
	outputScheme( scheme, token );
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
					fprintf( stderr, "Error: l%dc%d: ", line, column );
					if ( event == '\n' ) {
						fprintf( stderr, "statement incomplete\n" );
					} else {
						fprintf( stderr, "syntax error\n" );
						state = Flush;
					}
					ParserRebase( parser, P_ALL );
					break;
				case rvParserEncore:
					/* Here there are still active parser base threads.
					   We could systematically rebase the parser (P_BASE only),
					   in which case it would feedback results independently of
					   their start frame - pipeline mode.
					   Here we choose to ignore all intermediate results, and
					   keep threads deactivated until the longest one succeeds
					   (or fails).
					*/
					ParserRebase( parser, P_RESULTS );
					break;
				case rvParserNoMore:
					/* here all parser rules completed and at least one
					   base rules succeeded. It is up to the implementation
					   to call it an error or not
					*/
					if ( event != '\n' ) {
						fprintf( stderr, "Warning: premature completion\n" );
						state = Flush;
					}
					for ( listItem *i=parser->results; i!=NULL; i=i->next )
						outputSequence( parser, i->ptr );
					ParserRebase( parser, P_ALL );
					break;
			}
		}
		state = NextState( state, event );
	}
	while ( state != Exit );
	if ( event != EOF ) {
		fprintf( stderr, "yak$$\n" );
		ParserFrame( parser, EOF );
	}
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
	outputSequence
---------------------------------------------------------------------------*/
static void outputSpace( char * );
static void
outputSequence( Parser *parser, Sequence *sequence )
{
#ifdef DEBUG
	printf( "~~~~~~\n" );
#endif
	if ( parser->options & P_TOK ) {
		for ( listItem *i=sequence; i!=NULL; i=i->next ) {
			for ( listItem *j=i->ptr; j!=NULL; j=j->next ) {
				char *token = j->ptr;
				if ( is_space( token[ 0 ] ) )
					outputSpace( token );
				else
					printf( "%s", token );
				if ( j->next ) printf( " " );
			}
			if ( i->next ) printf( " " );
		}
	}
	else {
		CNStreamBegin( void *, stream, sequence )
		CNOnStreamValue( stream, Pair *, pair )
			if (( pair->name )) {
				printf( "%%%s:{", (char *) pair->name );
				CNStreamPush( stream, pair->value );
			}
			else {
				switch ((int) pair->value ) {
					case '\n': printf( "\\n" ); break;
					case '\t': printf( "\\t" ); break;
					default: printf( "%c", (int) pair->value );
				}
				CNStreamFlow( stream );
			}
		CNOnStreamEnd
			CNInStreamPop( stream, Pair *, pair )
				printf( "}" );
		CNStreamEnd( stream )
	}
	printf( "\n" );
#ifdef DEBUG
	printf( "~~~~~~\n" );
#endif
}

static void
outputSpace( char *sp )
{
	printf( "'" );
	for ( ; *sp; sp++ ) {
		switch ( *sp ) {
		case ' ':
			printf( " " );
			break;
		case '\t':
			printf( "\\t" );
			break;
		}
	}
	printf( "'" );
}
