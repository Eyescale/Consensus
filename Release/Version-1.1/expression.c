#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "expression.h"
#include "expression_private.h"
#include "traversal.h"

// #define DEBUG

//===========================================================================
//	bm_substantiate
//===========================================================================
static int bm_void( char *, BMContext * );

void
bm_substantiate( char *expression, BMContext *ctx )
/*
	Substantiates expression by verifying and/or instantiating all
	relationships involved. Note that, reassignment excepted, only
	new or previously deprecated relationship instances will be
	manifested.
*/
{
#ifdef DEBUG
	if ( bm_void( expression, ctx ) ) {
		fprintf( stderr, "VOID: %s\n", expression );
		return;
	}
	else fprintf( stderr, "bm_substantiate: %s {", expression );
#else
	if ( bm_void( expression, ctx ) ) return;
#endif

	struct {
		listItem *results;
		listItem *ndx;
	} stack = { NULL, NULL };
	listItem *sub[ 2 ] = { NULL, NULL }, *instances;

	CNInstance *e;
	CNDB *db = ctx->db;
	int level=1, ndx=0;
	char *p = expression;
	while ( *p && level ) {
		if ( p_filtered( p )  ) {
			// bm_void made sure we do have results
			sub[ ndx ] = bm_query( p, ctx );
			p = p_prune( PRUNE_TERM, p );
			continue;
		}
		switch ( *p ) {
		case '%':
			if ( !strncmp( p, "%?", 2 ) ) {
				// bm_void made sure we do have result
				e = bm_lookup( 0, p, ctx );
				sub[ ndx ] = newItem( e );
				p+=2; break;
			}
			// no break
		case '*':
			if ( !p[1] || strmatch( ":,)", p[1] ) ) {
				e = bm_register( ctx, p, NULL );
				sub[ ndx ] = newItem( e );
				p++; break;
			}
			// no break
		case '~':
			// bm_void made sure we do have results
			sub[ ndx ] = bm_query( p, ctx );
			p = p_prune( PRUNE_TERM, p+1 );
			break;
		case '(':
			level++;
			add_item( &stack.ndx, ndx );
			if ( ndx ) {
				addItem( &stack.results, sub[ 0 ] );
				ndx = 0; sub[ 0 ] = NULL;
			}
			p++; break;
		case ':': // should not happen
			break;
		case ',':
			if ( level == 1 ) { level=0; break; }
			ndx = 1;
			p++; break;
		case ')':
			level--;
			if ( !level ) break;
			switch ( ndx ) {
			case 0:
				instances = sub[ 0 ];
				break;
			case 1:
				instances = db_couple( sub, db );
				freeListItem( &sub[ 0 ] );
				freeListItem( &sub[ 1 ] );
				break;
			}
			ndx = (int) popListItem( &stack.ndx );
			sub[ ndx ] = instances;
			if ( ndx ) sub[ 0 ] = popListItem( &stack.results );
			p++; break;
		case '?':
		case '.':
			sub[ ndx ] = newItem( NULL );
			p++; break;
		default:
			e = bm_register( ctx, p, NULL );
			sub[ ndx ] = newItem( e );
			p = p_prune( PRUNE_IDENTIFIER, p );
		}
	}
#ifdef DEBUG
	if (( sub[ 0 ] )) {
		fprintf( stderr, "bm_substantiate: } first=" );
		cn_out( stderr, sub[0]->ptr, db );
		fprintf( stderr, "\n" );
	}
	else fprintf( stderr, "bm_substantiate: } no result\n" );
#endif
	freeListItem( &sub[ 0 ] );
}

static int
bm_void( char *expression, BMContext *ctx )
/*
	tests if expression is instantiable, ie. that
	. all inner queries have results
	. all the associations it contains can be made 
	  e.g. in case the CNDB is empty, '.' fails
	returns 0 if it is, 1 otherwise
*/
{
	// fprintf( stderr, "bm_void: %s\n", expression );
	CNDB *db = ctx->db;
	int level=1, empty=db_is_empty( db );
	char *p = expression;
	while ( *p && level ) {
		if ( p_filtered( p ) ) {
			if ( empty || !bm_feel( p, ctx, BM_CONDITION ) )
				return 1;
			p = p_prune( PRUNE_TERM, p );
			continue;
		}
		switch ( *p ) {
		case '%':
			if ( !strncmp( p, "%?", 2 ) ) {
				if ( bm_lookup( 0, p, ctx ) == NULL )
					return 1;
				p+=2; break;
			}
			// no break
		case '*':
			if ( !p[1] || strmatch( ":,)", p[1] ) )
				{ p++; break; }
			// no break
		case '~':
			if ( empty || !bm_feel( p, ctx, BM_CONDITION ) )
				return 1;
			p = p_prune( PRUNE_TERM, p+1 );
			break;
		case '(':
			level++;
			p++; break;
		case ':':
		case ',':
			if ( level == 1 ) { level=0; break; }
			p++; break;
		case ')':
			level--;
			p++; break;
		case '?':
		case '.':
			if ( empty ) return 1;
			p++; break;
		default:
			p = p_prune( PRUNE_IDENTIFIER, p );
		}
	}
	return 0;
}

//===========================================================================
//	bm_query
//===========================================================================
static BMTraverseCB query_CB;

listItem *
bm_query( char *expression, BMContext *ctx )
{
	listItem *results = NULL;
	bm_traverse( expression, ctx, query_CB, &results );
	return results;
}
static int
query_CB( CNInstance *e, BMContext *ctx, void *results )
{
	addIfNotThere((listItem **) results, e );
	return BM_CONTINUE;
}
//===========================================================================
//	bm_release
//===========================================================================
static BMTraverseCB release_CB;

void
bm_release( char *expression, BMContext *ctx )
{
	bm_traverse( expression, ctx, release_CB, NULL );
}
static int
release_CB( CNInstance *e, BMContext *ctx, void *user_data )
{
	db_deprecate( e, ctx->db );
	return BM_CONTINUE;
}

//===========================================================================
//	bm_inputf
//===========================================================================
static int bm_input( char *format, char *expression, BMContext * );

#define DEFAULT_FORMAT "_"

int
bm_inputf( char *format, listItem *args, BMContext *ctx )
/*
	Assumption: format starts and finishes with \" or \0
*/
{
	int event;
	if ( *format++ ) {
		while ( *format ) {
			switch (*format) {
			case '\"':
				goto RETURN;
			case '%':
				format++;
				if ( *format == '\0' )
					break;
				else if ( *format == '%' ) {
					event = getc( stdin );
					if ( event==EOF || event!='%' )
						goto RETURN;
				}
				else if ((args)) {
					Pair *pair = args->ptr;
					char *expression = pair->name;
					event = bm_input( format, expression, ctx );
					if ( event==EOF ) goto RETURN;
					args = args->next;
				}
				format++;
				break;
			default:;
				char q[ MAXCHARSIZE + 1 ];
				int delta = charscan( format, q );
				if ( delta ) {
					event = getc( stdin );
					if ( event==EOF || event!=q[0] )
						goto RETURN;
					format += delta;
				}
				else format++;
			}
		}
	}
	else {
		while (( args )) {
			Pair *pair = args->ptr;
			char *expression = pair->name;
			event = bm_input( DEFAULT_FORMAT, expression, ctx );
			if ( event == EOF ) break;
			args = args->next;
		}
	}
RETURN:
	if ( event == EOF ) {
		while (( args )) {
			Pair *pair = args->ptr;
			char *expression = pair->name;
			asprintf( &expression, "(*,%s)", expression );
			bm_release( expression, ctx );
			free( expression );
			args = args->next;
		}
	}
	else {
		switch ( *format ) {
		case '\0':
		case '\"':
			break;
		default:
			ungetc( event, stdin );
		}
	}
	return 0;
}
static char * readInput( FILE * );
static int
bm_input( char *format, char *expression, BMContext *ctx )
{
	char *input;
	int event;
	switch ( *format ) {
	case 'c':
		event = getc( stdin );
		if ( event == EOF ) {
			return EOF;
		}
		switch ( event ) {
		case '\0': asprintf( &input, "'\\0'" ); break;
		case '\t': asprintf( &input, "'\\t'" ); break;
		case '\n': asprintf( &input, "'\\n'" ); break;
		case '\'': asprintf( &input, "'\\\''" ); break;
		case '\\': asprintf( &input, "'\\\\'" ); break;
		default:
			if ( is_printable( event ) ) asprintf( &input, "'%c'", event );
			else asprintf( &input, "'\\x%.2X'", event );
		}
		break;
	case '_':
		input = readInput( stdin );
		if ( input == NULL ) return EOF;
		break;
	default:
		return 0;
	}
	asprintf( &expression, "((*,%s),%s)", expression, input );
	free( input );
	bm_substantiate( expression, ctx );
	free( expression );
	return 0;
}
static char *
readInput( FILE *stream )
{
	struct { listItem *first; } stack = { NULL };
	int	first = 1,
		informed = 0,
		level = 0;

	CNString *s = newString();

	BMInputBegin( stream, getc )
	on_( EOF )
		freeString( s );
		return NULL;
	in_( "base" ) bgn_
		on_( '#' )	do_( "#" )
		ons( "*%" )	do_( "" )	StringAppend( s, event );
		on_( '\'' )	do_( "char" )	StringAppend( s, event );
		on_( '(' )	do_( "expr" )	StringAppend( s, event );
		on_separator	do_( same )
		on_other	do_( "term" )	StringAppend( s, event );
		end
		in_( "#" ) bgn_
			on_( '\n' )	do_( "base" )
			on_other	do_( same )
			end
	in_( "char" ) bgn_
		on_( '\\' )	do_( "char\\" )
		on_printable	do_( "char_" )	StringAppend( s, event );
		end
		in_( "char\\" ) bgn_
			on_escapable	do_( "char_" )	StringAppend( s, event );
			on_( 'x' )	do_( "char\\x" ) StringAppend( s, event );
			end
		in_( "char\\x" ) bgn_
			on_xdigit	do_( "char\\x_" ) StringAppend( s, event );
			end
		in_( "char\\x" ) bgn_
			on_xdigit	do_( "char_" )	StringAppend( s, event );
			end
		in_( "char_" ) bgn_
			on_( '\'' )
				if ( level > 0 ) {
					do_( "expr" )	StringAppend( s, event );
							informed = 1;
				}
				else {	do_( "" ) }
			end
	in_( "expr" ) bgn_
		ons( " \t" )	do_( same )
		on_( '(' )
			if ( !informed ) {
				do_( same )	level++;
						StringAppend( s, event );
						add_item( &stack.first, first );
						first = 1; informed = 0;
			}
		on_( ',' )
			if ( first && informed && ( level > 0 )) {
				do_( same )	StringAppend( s, event );
						first = informed = 0;
			}
		on_( '\'' )
			if ( !informed ) {
				do_( "char" )	StringAppend( s, event );
			}
		on_( ')' )
			if ( informed && ( level > 0 )) {
				do_(( level==1 ? "" : "expr" )) level--;
						StringAppend( s, event );
						first = (int) popListItem( &stack.first );
			}
		on_separator	; // fail
		on_other
			if ( !informed ) {
				do_( "term" )	StringAppend( s, event );
			}
		end
	in_( "term" ) bgn_
		on_separator
			if ( level > 0 ) {
				do_( "expr" )	REENTER
						informed = 1;
			}
			else {	do_( "" )	ungetc( event, stream ); }
		on_other	do_( same )	StringAppend( s, event );
		end

	BMInputDefault
		in_none_sofar	fprintf( stderr, ">>>>>> B%%::BMInput: "
					"unknown state \"%s\" <<<<<<\n", state );
		in_other	do_( "base" )	StringReset( s, CNStringAll );
						freeListItem( &stack.first );
						first = 1; informed = level = 0;

	BMInputEnd

	char *p = StringFinish( s, 0 );
	StringReset( s, CNStringMode );
	freeListItem( &stack.first );
	freeString( s );
	return p;
}

//===========================================================================
//	bm_outputf
//===========================================================================
static void bm_output( char *format, char *expression, BMContext *);
static BMTraverseCB output_CB;
typedef struct {
	char *format;
	int first;
	CNInstance *last;
} OutputData;

int
bm_outputf( char *format, listItem *args, BMContext *ctx )
/*
	Assumption: format starts and finishes with \" or \0
*/
{
	if ( *format++ ) {
		while ( *format ) {
			switch ( *format ) {
			case '\"':
				goto RETURN;
			case '%':
				format++;
				if ( *format == '\0')
					break;
				else if ( *format == '%' )
					putchar( '%' );
				else if ((args)) {
					char *expression = args->ptr;
					bm_output( format, expression, ctx );
					args = args->next;
				}
				format++;
				break;
			default:;
				char q[ MAXCHARSIZE + 1 ];
				int delta = charscan(format,q);
				if ( delta ) {
					printf( "%c", *(unsigned char *)q );
					format += delta;
				}
				else format++;
			}
		}
	}
	else if ((args)) {
		do {
			char *expression = args->ptr;
			bm_output( DEFAULT_FORMAT, expression, ctx );
			args = args->next;
		} while ((args));
	}
	else printf( "\n" );
RETURN:
	return 0;
}
static void
bm_output( char *format, char *expression, BMContext *ctx )
/*
	outputs expression's results
	note that we rely on bm_traverse to eliminate doublons
*/
{
	OutputData data = { format, 1, NULL };
	bm_traverse( expression, ctx, output_CB, &data );
	if ( data.first )
		db_output( stdout, format, data.last, ctx->db );
	else {
		printf( ", " );
		db_output( stdout, DEFAULT_FORMAT, data.last, ctx->db );
		printf( " }" );
	}
}
static int
output_CB( CNInstance *e, BMContext *ctx, void *user_data )
{
	OutputData *data = user_data;
	if (( data->last )) {
		if ( data->first ) {
			if ( *data->format == 's' )
				printf( "\\" );
			printf( "{ " );
			data->first = 0;
		}
		else printf( ", " );
		db_output( stdout, DEFAULT_FORMAT, data->last, ctx->db );
	}
	data->last = e;
	return BM_CONTINUE;
}
