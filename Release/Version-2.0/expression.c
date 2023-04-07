#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "database.h"
#include "traverse.h"
#include "expression.h"
#include "instantiate.h"
#include "narrative.h"
#include "scour.h"
#include "proxy.h"
#include "eenov.h"

// #define DEBUG

//===========================================================================
//	bm_feel
//===========================================================================
CNInstance *
bm_feel( BMQueryType type, char *expression, BMContext *ctx )
{
	switch ( type ) {
	case BM_CONDITION: // special case: EEnoRV as-is
		if ( !strncmp(expression,"%<",2) && !p_filtered(expression) )
			return eenov_lookup( ctx, NULL, expression );
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
	return results;
}
static BMCBTake
scan_CB( CNInstance *e, BMContext *ctx, void *results )
{
	addIfNotThere((listItem **) results, e );
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
	db_deprecate( e, (CNDB *) user_data );
	return BM_CONTINUE;
}

//===========================================================================
//	bm_void
//===========================================================================
#include "void_traversal.h"

int
bm_void( char *expression, BMContext *ctx )
/*
	tests if expression is instantiable, ie. that
	. all inner queries have results
	. all the associations it contains can be made 
	returns 0 if it is instantiable, 1 otherwise
*/
{
	// fprintf( stderr, "bm_void: %s\n", expression );
	listItem *stack = NULL;

	BMTraverseData traverse_data;
	traverse_data.user_data = ctx;
	traverse_data.stack = &stack;
	traverse_data.done = INFORMED;

	void_traversal( expression, &traverse_data, FIRST );

	freeListItem( &stack );
	return ( traverse_data.done==2 );
}

//---------------------------------------------------------------------------
//	void_traversal
//---------------------------------------------------------------------------
BMTraverseCBSwitch( void_traversal )
case_( term_CB )
	if is_f( FILTERED ) {
		if ( !bm_feel( BM_CONDITION, p, ctx ) )
			_return( 2 )
		_prune( BM_PRUNE_TERM ) }
	_break
case_( dereference_CB )
	int target = bm_scour( p, PMARK );
	if ( target&PMARK && target!=PMARK ) {
		fprintf( stderr, ">>>>> B%%:: Warning: bm_void, at '%s' - "
		"dubious combination of query terms with %%!\n", p ); }
	if ( !bm_feel( BM_CONDITION, p, ctx ) )
		_return( 2 )
	_prune( BM_PRUNE_TERM )
case_( register_variable_CB )
	int nope = 0;
	switch ( p[1] ) {
	case '?': nope = !bm_context_lookup( ctx, "?" ); break;
	case '!': nope = !bm_context_lookup( ctx, "!" ); break; }
	if ( nope )
		_return( 2 )
	_break
BMTraverseCBEnd

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
	case '_':
		input = bm_read( BM_INPUT, stdin );
		if ( !input ) return EOF;
		break;
	default:
		return 0; }

	bm_instantiate_input( input, arg, ctx );
	free( input );

	return 0;
}

//===========================================================================
//	bm_outputf
//===========================================================================
static int bm_output( int type, char *expression, BMContext *);
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
					args = args->next; }
				fmt+=2; break;
			default:
				delta = charscan( fmt, &q );
				if ( delta ) {
					printf( "%c", q.value );
					fmt += delta; }
				else fmt++;
			} } }
	else {
		if ((args)) do {
			char *arg = args->ptr;
			bm_output( DEFAULT_TYPE, arg, ctx );
			args = args->next;
		} while ((args));
		printf( "\n" ); }
RETURN:
	return 0;
}
static int
bm_output( int type, char *arg, BMContext *ctx )
/*
	outputs arg-expression's results
	note that we rely on bm_query to eliminate doublons
*/
{
	// special case: EEnoRV as-is
	if ( !strncmp(arg,"%<",2) && !p_filtered(arg) )
		return eenov_output( ctx, type, arg );
	else if ( *arg=='\'' && type=='s' ) {
		char_s q;
		if ( charscan( arg+1, &q ) )
			fprintf( stdout, "%c", q.value );
		return 0; }

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
			break; }
	else {
		printf( ", " );
		db_outputf( stdout, db, "%_", data.last );
		printf( " }" ); }

	return 0;
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
			data->first = 0; }
		else printf( ", " );
		db_outputf( stdout, db, "%_", data->last ); }
	data->last = e;
	return BM_CONTINUE;
}
