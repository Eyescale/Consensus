#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "database.h"
#include "narrative.h"
#include "expression.h"

// #define DEBUG

//===========================================================================
//	bm_feel
//===========================================================================
CNInstance *
bm_feel( BMQueryType type, char *expression, BMContext *ctx )
{
	return bm_query( type, expression, ctx, NULL, NULL );

}

//===========================================================================
//	bm_release
//===========================================================================
static BMQueryCB release_CB;

void
bm_release( char *expression, BMContext *ctx )
{
	bm_query( BM_CONDITION, expression, ctx, release_CB, NULL );
}
static BMCB_
release_CB( CNInstance *e, BMContext *ctx, void *user_data )
{
	CNDB *db = BMContextDB( ctx );
	db_deprecate( e, db );
	return BM_CONTINUE;
}

//===========================================================================
//	bm_assign_op
//===========================================================================
#define s_add( str ) for ( char *p=str; *p; StringAppend(s,*p++) );

int
bm_assign_op( int op, char *expression, BMContext *ctx, int *marked )
/*
   Assumption: expression is either one of the following
	 : variable : value 
	 : variable : ~.
*/
{
	CNInstance *e;
	int success = 0;
	CNString *s = newString();
	char *p = expression+1; // skip leading ':'
	char *q = p_prune( PRUNE_FILTER, p );
	if ( strcmp( q+1, "~." ) ) {
		// build ((*,variable),value)
		s_add( "((*," );
		for ( ; p!=q; p++ ) StringAppend( s, *p );
		s_add( ")," );
		for ( p++; *p; p++ ) StringAppend( s, *p );
		s_add( ")" );
		p = StringFinish( s, 0 );
		// perform operation
		switch ( op ) {
		case IN:
			e = bm_feel( BM_CONDITION, p, ctx );
			success = bm_context_mark( ctx, p, e, marked );
			break;
		case ON:
			e = bm_feel( BM_INSTANTIATED, p, ctx );
			success = bm_context_mark( ctx, p, e, marked );
			break;
		case DO:
			bm_instantiate( p, ctx );
			break;
		}
	}
	else { 	// We have : variable : ~.
		// build (*,variable)
		s_add( "(*," );
		for ( ; p!=q; p++ )
			StringAppend( s, *p );
		s_add( ")" );
		p = StringFinish( s, 0 );
		// perform operation
		switch ( op ) {
		case IN:
			e = bm_feel( BM_CONDITION, p, ctx );
			if (( e ) && !db_coupled( 0, e, BMContextDB(ctx) ))
				success = bm_context_mark( ctx, p, e, marked );
			break;
		case ON:
			e = bm_feel( BM_INSTANTIATED, p, ctx );
			if (( e ) && !db_coupled( 0, e, BMContextDB(ctx) ))
				success = bm_context_mark( ctx, p, e, marked );
			break;
		case DO:
			e = bm_feel( BM_CONDITION, p, ctx );
			if (( e )) db_uncouple( e, BMContextDB(ctx) );
			else bm_instantiate( p, ctx );
			break;
		}
	}
	freeString( s );
	return success;
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
	int event, delta;
	char_s q;
	if ( *format ) {
		format++; // skip opening double-quote
		while ( *format ) {
			switch (*format) {
			case '\"':
				goto RETURN;
			case '%':
				format++;
				if ( *format == '\0' )
					break;
				else if ( *format == '%' ) {
					event = fgetc( stdin );
					if ( event==EOF || event!='%' )
						goto RETURN;
				}
				else if ((args)) {
					char *expression = args->ptr;
					event = bm_input( format, expression, ctx );
					if ( event==EOF ) goto RETURN;
					args = args->next;
				}
				format++;
				break;
			default:
				delta = charscan( format, &q );
				if ( delta ) {
					event = fgetc( stdin );
					if ( event==EOF || event!=q.value )
						goto RETURN;
					format += delta;
				}
				else format++;
			}
		}
	}
	else {
		while (( args )) {
			char *expression = args->ptr;
			event = bm_input( DEFAULT_FORMAT, expression, ctx );
			if ( event == EOF ) break;
			args = args->next;
		}
	}
RETURN:
	if ( event == EOF ) {
		while (( args )) {
			char *expression = args->ptr;
			asprintf( &expression, "(*,%s)", expression );
			bm_release( expression, ctx );
			free( expression );
			args = args->next;
		}
	}
	else if ( *format && *format!='\"' ) {
		ungetc( event, stdin );
	}
	return 0;
}
static int
bm_input( char *format, char *expression, BMContext *ctx )
{
	char *input;
	int event;
	switch ( *format ) {
	case 'c':
		event = fgetc( stdin );
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
		input = bm_read( CN_INSTANCE, stdin );
		if ( input == NULL ) return EOF;
		break;
	default:
		return 0;
	}
	asprintf( &expression, "((*,%s),%s)", expression, input );
	free( input );
	bm_instantiate( expression, ctx );
	free( expression );
	return 0;
}

//===========================================================================
//	bm_outputf
//===========================================================================
static void bm_output( char *format, char *expression, BMContext *);
static BMQueryCB output_CB;
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
	int delta; char_s q;
	if ( *format ) {
		format++; // skip opening double-quote
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
			default:
				delta = charscan( format, &q );
				if ( delta ) {
					printf( "%c", q.value );
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
	note that we rely on bm_query to eliminate doublons
*/
{
	OutputData data = { format, 1, NULL };
	bm_query( BM_CONDITION, expression, ctx, output_CB, &data );
	CNDB *db = BMContextDB( ctx );
	if ( data.first )
		db_output( stdout, format, data.last, db );
	else {
		printf( ", " );
		db_output( stdout, DEFAULT_FORMAT, data.last, db );
		printf( " }" );
	}
}
static BMCB_
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
		CNDB *db = BMContextDB( ctx );
		db_output( stdout, DEFAULT_FORMAT, data->last, db );
	}
	data->last = e;
	return BM_CONTINUE;
}
