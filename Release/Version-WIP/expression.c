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
	CNDB *db = BMContextDB( ctx );
	bm_query( BM_CONDITION, expression, ctx, release_CB, db );
}
static BMCBTake
release_CB( CNInstance *e, BMContext *ctx, void *user_data )
{
	db_deprecate( e, (CNDB *) user_data );
	return BM_CONTINUE;
}

//===========================================================================
//	bm_proxy_op
//===========================================================================
void
bm_proxy_op( char *expression, BMContext *ctx )
{
	char *p = expression;
	BMQueryCB *op = ( *p=='@' ) ?
		bm_activate : bm_deactivate;
	p += 2;
	if ( *p=='{' )
		do {	p++; bm_query( BM_CONDITION, p, ctx, op, NULL );
			p = p_prune( PRUNE_TERM, p );
		} while ( *p!='}' );
	else
		bm_query( BM_CONDITION, p, ctx, op, NULL );
}

//===========================================================================
//	bm_inputf
//===========================================================================
static int bm_input( int type, char *expression, BMContext * );

#define DEFAULT_TYPE '_'

int
bm_inputf( char *fmt, listItem *args, BMContext *ctx )
/*
	Assumption: fmt starts and finishes with \" or \0
*/
{
	int event=0, delta;
	char_s q;
	if ( *fmt ) {
		fmt++; // skip opening double-quote
		while ( *fmt ) {
			switch (*fmt) {
			case '"':
				goto RETURN;
			case '%':
				if ( !fmt[1] ) goto RETURN;
				else if ( fmt[1]=='%' ) {
					event = fgetc(stdin);
					if ( event==EOF ) goto RETURN;
					else if ( event!='%' ) {
						ungetc( event, stdin );
						goto RETURN; }
				}
				else if ((args)) {
					char *expression = args->ptr;
					event = bm_input( fmt[1], expression, ctx );
					if ( event==EOF ) goto RETURN;
					args = args->next;
				}
				fmt+=2; break;
			default:
				delta = charscan( fmt, &q );
				if ( delta ) {
					event = fgetc( stdin );
					if ( event==EOF ) goto RETURN;
					else if ( event!=q.value ) {
						ungetc( event, stdin );
						fmt += delta;
					}
				}
				else fmt++;
			}
		}
	}
	else {
		while (( args )) {
			char *expression = args->ptr;
			event = bm_input( DEFAULT_TYPE, expression, ctx );
			if ( event == EOF ) break;
			args = args->next;
		}
	}
RETURN:
	if ( event==EOF ) {
		while (( args )) {
			char *expression = args->ptr;
			asprintf( &expression, "(*,%s)", expression );
			bm_release( expression, ctx );
			free( expression );
			args = args->next;
		}
	}
	return 0;
}
static int
bm_input( int type, char *expression, BMContext *ctx )
{
	char *input;
	int event;
	switch ( type ) {
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
		input = bm_read( BM_INPUT, stdin );
		if ( !input ) return EOF;
		break;
	default:
		return 0;
	}
	asprintf( &expression, "((*,%s),%s)", expression, input );
	free( input );
	bm_instantiate( expression, ctx, NULL );
	free( expression );
	return 0;
}

//===========================================================================
//	bm_outputf
//===========================================================================
static void bm_output( int type, char *expression, BMContext *);
static BMQueryCB output_CB;
typedef struct {
	int type;
	int first;
	CNInstance *last;
} OutputData;

int
bm_outputf( char *fmt, listItem *args, BMContext *ctx )
/*
	Assumption: fmt starts and finishes with \" or \0
*/
{
	int delta; char_s q;
	if ( *fmt ) {
		fmt++; // skip opening double-quote
		while ( *fmt ) {
			switch ( *fmt ) {
			case '"':
				goto RETURN;
			case '%':
				if ( !fmt[1] ) break;
				else if ( fmt[1]=='%' )
					putchar( '%' );
				else if ((args)) {
					char *arg = args->ptr;
					bm_output( fmt[1], arg, ctx );
					args = args->next;
				}
				fmt+=2; break;
			default:
				delta = charscan( fmt, &q );
				if ( delta ) {
					printf( "%c", q.value );
					fmt += delta;
				}
				else fmt++;
			}
		}
	}
	else if ((args)) {
		do {
			char *arg = args->ptr;
			bm_output( DEFAULT_TYPE, arg, ctx );
			args = args->next;
		} while ((args));
	}
	else printf( "\n" );
RETURN:
	return 0;
}
static void
bm_output( int type, char *arg, BMContext *ctx )
/*
	outputs arg-expression's results
	note that we rely on bm_query to eliminate doublons
*/
{
	OutputData data = { type, 1, NULL };
	bm_query( BM_CONDITION, arg, ctx, output_CB, &data );
	CNDB *db = BMContextDB( ctx );
	if ( data.first )
		switch ( type ) {
		case 's':
			db_outputf( stdout, db, "%s", data.last );
			break;
		default:
			db_outputf( stdout, db, "%_", data.last );
			break;
		}
	else {
		printf( ", " );
		db_outputf( stdout, db, "%_", data.last );
		printf( " }" );
	}
}
static BMCBTake
output_CB( CNInstance *e, BMContext *ctx, void *user_data )
{
	OutputData *data = user_data;
	if (( data->last )) {
		CNDB *db = BMContextDB( ctx );
		if ( data->first ) {
			if ( data->type=='s' )
				printf( "\\{ " );
			else printf( "{ " );
			data->first = 0;
		}
		else printf( ", " );
		db_outputf( stdout, db, "%_", data->last );
	}
	data->last = e;
	return BM_CONTINUE;
}
