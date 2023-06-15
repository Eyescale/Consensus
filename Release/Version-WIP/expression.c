#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "database.h"
#include "traverse.h"
#include "expression.h"
#include "instantiate.h"
#include "narrative.h"
#include "eenov.h"

// #define DEBUG

//===========================================================================
//	bm_feel
//===========================================================================
CNInstance *
bm_feel( int type, char *expression, BMContext *ctx )
{
	switch ( type ) {
	case BM_CONDITION: // special case: EEnoRV as-is
		if ( !strncmp(expression,"%<",2) && !p_filtered(expression) ) {
			return eenov_lookup( ctx, NULL, expression ); }
		// no break
	default:
		return bm_query( type, expression, ctx, NULL, NULL ); }
}

//===========================================================================
//	bm_scan
//===========================================================================
static BMQueryCB scan_CB;

listItem *
bm_scan( char *expression, BMContext *ctx )
{
	listItem *results = NULL;
	bm_query( BM_CONDITION, expression, ctx, scan_CB, &results );
	reorderListItem( &results );
	return results;
}
static BMCBTake
scan_CB( CNInstance *e, BMContext *ctx, void *user_data )
{
	listItem **results = user_data;
	addItem( results, e );
	return BM_CONTINUE;
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
	CNDB *db = user_data;
	if ( !db_deprecated( e, db ) )
		db_deprecate( e, db );
	return BM_CONTINUE;
}

//===========================================================================
//	bm_inputf
//===========================================================================
static int bm_input( int type, char *arg, BMContext * );

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
						goto RETURN; } }
				else if ((args)) {
					char *arg = args->ptr;
					event = bm_input( fmt[1], arg, ctx );
					if ( event==EOF ) goto RETURN;
					args = args->next; }
				fmt+=2; break;
			default:
				delta = charscan( fmt, &q );
				if ( delta ) {
					event = fgetc( stdin );
					if ( event==EOF ) goto RETURN;
					else if ( event!=q.value ) {
						ungetc( event, stdin );
						fmt += delta; } }
				else fmt++; } } }
	else {
		while (( args )) {
			char *arg = args->ptr;
			event = bm_input( DEFAULT_TYPE, arg, ctx );
			if ( event==EOF ) break;
			else args = args->next; } }
RETURN:
	if ( event==EOF ) {
		while (( args )) {
			char *arg = args->ptr;
			bm_instantiate_input( NULL, arg, ctx );
			args = args->next; } }
	return 0;
}
static int
bm_input( int type, char *arg, BMContext *ctx )
{
	char *input = NULL;
	int event = 0;
	switch ( type ) {
	case 'c':
		event = fgetc( stdin );
		if ( event == EOF )
			return EOF;
		switch ( event ) {
		case '\0': asprintf( &input, "'\\0'" ); break;
		case '\t': asprintf( &input, "'\\t'" ); break;
		case '\n': asprintf( &input, "'\\n'" ); break;
		case '\'': asprintf( &input, "'\\\''" ); break;
		case '\\': asprintf( &input, "'\\\\'" ); break;
		default:
			if ( is_printable( event ) ) asprintf( &input, "'%c'", event );
			else asprintf( &input, "'\\x%.2X'", event ); }
		break;
	default:
		input = bm_read( BM_INPUT, stdin );
		if ( !input ) return EOF; }

	bm_instantiate_input( input, arg, ctx );
	free( input );
	return 0;
}

//===========================================================================
//	bm_outputf / bm_out_put / bm_out_flush
//===========================================================================
static int bm_output( FILE *, int type, char *expression, BMContext *);

int
bm_outputf( FILE *stream, char *fmt, listItem *args, BMContext *ctx )
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
					fprintf( stream, "%%" );
				else if ((args)) {
					char *arg = args->ptr;
					bm_output( stream, fmt[1], arg, ctx );
					args = args->next; }
				fmt+=2; break;
			default:
				delta = charscan( fmt, &q );
				if ( delta ) {
					fprintf( stream, "%c", q.value );
					fmt += delta; }
				else fmt++; } } }
	else if (( args )) {
		for ( listItem *i=args; i!=NULL; i=i->next ) {
			bm_output( stream, DEFAULT_TYPE, i->ptr, ctx );
			fprintf( stream, "\n" ); } }
	else fprintf( stream, "\n" );
RETURN:
	return 0;
}

//---------------------------------------------------------------------------
//	bm_output
//---------------------------------------------------------------------------
static BMQueryCB output_CB;

static int
bm_output( FILE *stream, int type, char *arg, BMContext *ctx )
/*
	outputs arg-expression's results
	note that we rely on bm_query to eliminate doublons
*/
{
	// special case: single quote & type 's' or '$'
	if ( *arg=='\'' && type!=DEFAULT_TYPE ) {
		char_s q;
		if ( charscan( arg+1, &q ) )
			fprintf( stream, "%c", q.value );
		return 0; }

	OutputData data = { stream, type, 1, NULL };

	// special case: EEnoRV as-is
	if ( !strncmp(arg,"%<",2) && !p_filtered(arg) )
		return eenov_output( arg, ctx, &data );

	bm_query( BM_CONDITION, arg, ctx, output_CB, &data );
	return bm_out_flush( &data, BMContextDB(ctx) );
}
static BMCBTake
output_CB( CNInstance *e, BMContext *ctx, void *user_data )
{
	bm_out_put( user_data, e, BMContextDB(ctx) );
	return BM_CONTINUE;
}

//---------------------------------------------------------------------------
//	bm_out_put / bm_out_flush
//---------------------------------------------------------------------------
void
bm_out_put( OutputData *data, CNInstance *e, CNDB *db )
{
	if (( data->last )) {
		FILE *stream = data->stream;
		if ( data->type=='$' )
			db_outputf( stream, db, "%_", data->last );
		else {
			if ( data->first ) {
				switch ( data->type ) {
				case 's': fprintf( stream, "\\{" ); break;
				default: fprintf( stream, "{" ); }
				data->first = 0; }
			else fprintf( stream, "," );
			db_outputf( stream, db, " %_", data->last ); } }
	data->last = e;
}
int
bm_out_flush( OutputData *data, CNDB *db )
{
	FILE *stream = data->stream;
	if ( data->type=='$' )
		db_outputf( stream, db, "%_", data->last );
	else if ( !data->first )
		db_outputf( stream, db, ", %_ }", data->last );
	else if ( data->type=='s' )
		db_outputf( stream, db, "%s", data->last );
	else	db_outputf( stream, db, "%_", data->last );
	return 0;
}
