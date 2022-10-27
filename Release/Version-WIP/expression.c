#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "database.h"
#include "locate.h"
#include "narrative.h"
#include "expression.h"

// #define DEBUG

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
static BMTraverseCB feel;
static BMTraverseCB
	feel_CB, sound_CB, touch_CB;
#define case_( func ) \
	} static BMCBTake func( BMTraverseData *traverse_data, char **q, int flags, int f_next ) { \
		BMContext *ctx = traverse_data->user_data; char *p = *q;

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
	memset( &traverse_data, 0, sizeof(traverse_data) );
	traverse_data.user_data = ctx;
	traverse_data.stack = &stack;
	traverse_data.done = INFORMED;

	BMTraverseCB **table = (BMTraverseCB **) traverse_data.table;
	table[ BMTermCB ]		= feel_CB;
	table[ BMNotCB ]		= sound_CB;
	table[ BMDereferenceCB ]	= sound_CB;
	table[ BMSubExpressionCB ]	= sound_CB;
	table[ BMRegisterVariableCB ]	= touch_CB;
	bm_traverse( expression, &traverse_data, FIRST );

	freeListItem( &stack );
	return ( traverse_data.done==2 );
}

//---------------------------------------------------------------------------
//	bm_void_traversal
//---------------------------------------------------------------------------
BMTraverseCBSwitch( bm_void_traversal )
case_( feel_CB )
	if is_f( FILTERED ) {
		if ( !bm_feel( BM_CONDITION, p, ctx ) )
			_return( 2 )
		_prune( BM_PRUNE_TERM ) }
	_break
case_( sound_CB )
	int target = bm_scour( p, PMARK );
	if ( target&PMARK && target!=PMARK ) {
		fprintf( stderr, ">>>>> B%%:: Warning: bm_void, at '%s' - "
		"dubious combination of query terms with %%!\n", p );
	}
	if ( !bm_feel( BM_CONDITION, p, ctx ) )
		_return( 2 )
	_prune( BM_PRUNE_TERM )
case_( touch_CB )
	switch ( p[1] ) {
	case '.':
		if ( !BMContextParent( ctx ) )
			_return( 2 )
		break;
	case '?':
	case '!':
		if ( !bm_context_lookup( ctx, ((char_s)(int)p[1]).s ) )
			_return( 2 )
		break; }
	_break
BMTraverseCBEnd

//===========================================================================
//	bm_proxy_op
//===========================================================================
void
bm_proxy_op( char *expression, BMContext *ctx )
{
	if ( !expression || !(*expression)) return;

	bm_context_check( ctx ); // remove dangling connections
	char *p = expression;
	BMQueryCB *op = ( *p=='@' ) ?
		bm_activate : bm_deactivate;
	p += 2;
	if ( *p!='{' )
		bm_query( BM_CONDITION, p, ctx, op, NULL );
	else do {
		p++; bm_query( BM_CONDITION, p, ctx, op, NULL );
		p = p_prune( PRUNE_TERM, p );
	} while ( *p!='}' );
}

//===========================================================================
//	bm_proxy_scan
//===========================================================================
listItem *
bm_proxy_scan( char *expression, BMContext *ctx )
/*
	return all context's active connections (proxies) matching expression
*/
{
	if ( !expression || !(*expression)) return NULL;

	bm_context_check( ctx ); // remove dangling connections
	listItem *results = NULL;

	CNDB *db = BMContextDB( ctx );
	BMQueryData data;
	memset( &data, 0, sizeof(BMQueryData) );
	data.type = BM_CONDITION;
	data.ctx = ctx;
	data.star = db_star( db );

	for ( listItem *i=BMContextActive(ctx)->value; i!=NULL; i=i->next ) {
		CNInstance *proxy = i->ptr;
		if ( xp_verify( proxy, expression, &data ) )
			addItem( &results, proxy ); }
	return results;
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
