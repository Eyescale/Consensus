#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "database.h"
#include "narrative.h"
#include "expression.h"
#include "traverse.h"
#include "locate.h"

// #define DEBUG

static int bm_void( char *, BMContext * );
static listItem *bm_couple( listItem *sub[2], BMContext * );
static CNInstance *bm_literal( char *, BMContext * );
static listItem *bm_scan( char *, BMContext * );

//===========================================================================
//	bm_instantiate
//===========================================================================
static BMTraverseCB
	filter_CB, collect_CB, bgn_set_CB, end_set_CB, bgn_pipe_CB, end_pipe_CB,
	open_CB, close_CB, decouple_CB, mark_register_CB, literal_CB,
	wildcard_CB, identifier_CB, signal_CB, end_CB;
typedef struct {
	listItem *sub[ 2 ];
	listItem *results;
	BMContext *ctx;
	int empty;
} BMInstantiateData;
#define CB_( func ) \
	} static BMCB_ func( BMTraverseData *traverse_data, char *p, int flags ) { \
		BMInstantiateData *data = traverse_data->user_data;
#define current \
	( is_f( FIRST ) ? 0 : 1 )

void
bm_instantiate( char *expression, BMContext *ctx )
{
#ifdef DEBUG
	if ( bm_void( expression, ctx ) ) {
		fprintf( stderr, ">>>>> B%%: bm_instantiate(): VOID: %s\n", expression );
		exit( -1 );
	}
	else fprintf( stderr, "bm_instantiate: %s {\n", expression );
#endif
	BMInstantiateData data;
	memset( &data, 0, sizeof(data) );
	data.empty = db_is_empty( 0, BMContextDB(ctx) );
	data.ctx = ctx;

	BMTraverseData traverse_data;
	memset( &traverse_data, 0, sizeof(traverse_data) );
	traverse_data.user_data = &data;

	BMTraverseCB **table = (BMTraverseCB **) traverse_data.table;
	table[ BMPreemptCB ]		= filter_CB;
	table[ BMNegatedCB ]		= collect_CB;
	table[ BMDereferenceCB ]	= collect_CB;
	table[ BMBgnSetCB ]		= bgn_set_CB;
	table[ BMEndSetCB ]		= end_set_CB;
	table[ BMBgnPipeCB ]		= bgn_pipe_CB;
	table[ BMEndPipeCB ]		= end_pipe_CB;
	table[ BMSubExpressionCB ]	= collect_CB;
	table[ BMModCharacterCB ]	= identifier_CB;
	table[ BMStarCharacterCB ]	= identifier_CB;
	table[ BMMarkRegisterCB ]	= mark_register_CB;
	table[ BMLiteralCB ]		= literal_CB;
	table[ BMOpenCB ]		= open_CB;
	table[ BMDecoupleCB ]		= decouple_CB;
	table[ BMCloseCB ]		= close_CB;
	table[ BMCharacterCB ]		= identifier_CB;
	table[ BMWildCardCB ]		= wildcard_CB;
	table[ BMIdentifierCB ]		= identifier_CB;
	table[ BMSignalCB ]		= signal_CB;

	listItem *stack = NULL;
	bm_traverse( expression, &traverse_data, &stack, FIRST );
	freeListItem( &stack );
	if ( traverse_data.done == 2 ) {
		freeListItem( &data.sub[ 1 ] );
		listItem *instances;
		listItem **results = &data.results;
		while (( instances = popListItem(results) ))
			freeListItem( &instances );
	}
#ifdef DEBUG
	if (( data.sub[ 0 ] )) {
		fprintf( stderr, "bm_instantiate: } " );
		if ( traverse_data.done == 2 )
			fprintf( stderr, "***INCOMPLETE***" );
		else {
			CNDB *db = BMContextDB( ctx );
			CNInstance *e = data.sub[ 0 ]->ptr;
			db_outputf( stderr, "first=%_", db, e ); }
		fprintf( stderr, "\n" ); }
	else fprintf( stderr, "bm_instantiate: } no result\n" );
#endif
	freeListItem( &data.sub[ 0 ] );
}

BMTraverseCBBegin( bm_instantiate_traversal )
CB_( filter_CB )
	if ( p_filtered( p ) )
		return collect_CB( traverse_data, p, flags );
	return BM_CONTINUE;
CB_( collect_CB )
	if (!( data->sub[ current ] = bm_scan( p, data->ctx ) ))
		traverse_data->done = 2;
	else {
		traverse_data->p = p_prune( PRUNE_TERM, p );
		traverse_data->flags = f_set( INFORMED ); }
	return BM_DONE;
CB_( bgn_set_CB )
	if (!is_f(FIRST)) {
		addItem( &data->results, data->sub[ 0 ] );
		data->sub[ 0 ] = NULL; }
	addItem( &data->results, NULL );
	return BM_CONTINUE;
CB_( end_set_CB )
	/* Assumption:	!is_f(LEVEL)
	*/
	listItem *instances = popListItem( &data->results );
	instances = catListItem( instances, data->sub[ 0 ] );
	flags = traverse_data->f_next;
	if is_f( FIRST )
		data->sub[ 0 ] = instances;
	else {
		data->sub[ 0 ] = popListItem( &data->results );
		data->sub[ 1 ] = instances; }
	return BM_CONTINUE;
CB_( bgn_pipe_CB )
	bm_push_mark( data->ctx, '!', data->sub[ current ] );
	addItem( &data->results, data->sub[ 0 ] );
	if (!is_f(FIRST)) {
		addItem( &data->results, data->sub[ 1 ] );
		data->sub[ 1 ] = NULL; }
	data->sub[ 0 ] = NULL;
	return BM_CONTINUE;
CB_( end_pipe_CB )
	bm_pop_mark( data->ctx, '!' );
	freeListItem( &data->sub[ 0 ] );
	flags = traverse_data->f_next;
	if (!is_f(FIRST))
		data->sub[ 1 ] = popListItem( &data->results );
	data->sub[ 0 ] = popListItem( &data->results );
	return BM_CONTINUE;
CB_( mark_register_CB )
	if ( p[1]=='?' ) {
		CNInstance *e = bm_context_lookup( data->ctx, "?" );
		if ( !e ) { traverse_data->done=2; return BM_DONE; }
		data->sub[ current ] = newItem( e ); }
	else if ( p[1]=='!' ) {
		listItem *i = bm_context_lookup( data->ctx, "!" );
		if ( !i ) { traverse_data->done=2; return BM_DONE; }
		listItem **sub = &data->sub[ current ];
		for ( ; i!=NULL; i=i->next ) addItem( sub, i->ptr ); }
	return BM_CONTINUE;
CB_( literal_CB )
	CNInstance *e = bm_literal( p, data->ctx );
	data->sub[ current ] = newItem( e );
	return BM_CONTINUE;
CB_( open_CB )
	if (!is_f(FIRST)) {
		addItem( &data->results, data->sub[ 0 ] );
		data->sub[ 0 ] = NULL; }
	return BM_CONTINUE;
CB_( decouple_CB )
	if (!is_f(LEVEL)) {	// Assumption: is_f(SET)
		listItem **results = &data->results;
		listItem *instances = popListItem( results );
		addItem( results, catListItem( instances, data->sub[ 0 ] ));
		data->sub[ 0 ] = NULL; }
	return BM_CONTINUE;
CB_( close_CB )
	listItem *instances = is_f( FIRST ) ?
		data->sub[ 0 ] : bm_couple( data->sub, data->ctx );
	if (!is_f(FIRST)) {
		freeListItem( &data->sub[ 0 ] );
		freeListItem( &data->sub[ 1 ] ); }
	flags = traverse_data->f_next;
	if is_f( FIRST )
		data->sub[ 0 ] = instances;
	else {
		data->sub[ 0 ] = popListItem( &data->results );
		data->sub[ 1 ] = instances; }
	return BM_CONTINUE;
CB_( wildcard_CB )
	if ( data->empty ) { traverse_data->done=2; return BM_DONE; }
	data->sub[ current ] = newItem( NULL );
	return BM_CONTINUE;
CB_( identifier_CB )
	CNInstance *e = bm_register( data->ctx, p );
	data->sub[ current ] = newItem( e );
	return BM_CONTINUE;
CB_( signal_CB )
	CNInstance *e = data->sub[ current ]->ptr;
	db_signal( e, BMContextDB(data->ctx) );
	return BM_CONTINUE;
BMTraverseCBEnd

//===========================================================================
//	bm_void
//===========================================================================
static BMTraverseCB
	feel_CB, check_CB, touch_CB, sound_CB;
typedef struct {
	BMContext *ctx;
	int empty;
} BMVoidData;
#undef CB_
#define CB_( func ) \
	} static BMCB_ func( BMTraverseData *traverse_data, char *p, int flags ) { \
		BMVoidData *data = traverse_data->user_data;

static int
bm_void( char *expression, BMContext *ctx )
/*
	tests if expression is instantiable, ie. that
	. all inner queries have results
	. all the associations it contains can be made 
	  e.g. in case the CNDB is empty, '.' fails
	returns 0 if it is instantiable, 1 otherwise
*/
{
	// fprintf( stderr, "bm_void: %s\n", expression );
	BMVoidData data;
	memset( &data, 0, sizeof(data) );
	data.empty = db_is_empty( 0, BMContextDB(ctx) );
	data.ctx = ctx;

	BMTraverseData traverse_data;
	memset( &traverse_data, 0, sizeof(traverse_data) );
	traverse_data.user_data = &data;

	BMTraverseCB **table = (BMTraverseCB **) traverse_data.table;
	table[ BMPreemptCB ]		= feel_CB;
	table[ BMNegatedCB ]		= check_CB;
	table[ BMDereferenceCB ]	= check_CB;
	table[ BMSubExpressionCB ]	= check_CB;
	table[ BMMarkRegisterCB ]	= touch_CB;
	table[ BMWildCardCB ]		= sound_CB;

	listItem *stack = NULL;
	bm_traverse( expression, &traverse_data, &stack, FIRST );
	freeListItem( &stack );
	return ( traverse_data.done==2 );
}

static BMTraverseCB feel;
BMTraverseCBBegin( bm_void_traversal )
CB_( feel_CB )
	if ( p_filtered( p ) )
		return feel( traverse_data, p, flags );
	return BM_CONTINUE;
CB_( check_CB )
	int target = bm_scour( p, EMARK );
	if ( target&EMARK && target!=EMARK ) {
			fprintf( stderr, ">>>>> B%%:: Warning: bm_void, at '%s' - "
			"dubious combination of query terms with %%!\n", p );
	}
	return feel( traverse_data, p, flags );
CB_( touch_CB )
	if ( p[1]=='?' && !bm_context_lookup( data->ctx, "?" ) )
		{ traverse_data->done=2; return BM_DONE; }
	return BM_CONTINUE;
CB_( sound_CB )
	if ( data->empty )
		{ traverse_data->done=2; return BM_DONE; }
	return BM_CONTINUE;
BMTraverseCBEnd

static BMCB_
feel( BMTraverseData *traverse_data, char *p, int flags )
{
	BMVoidData *data = traverse_data->user_data;
	if ( data->empty || !bm_feel( BM_CONDITION, p, data->ctx ) )
		traverse_data->done = 2;
	else {
		traverse_data->p = p_prune( PRUNE_TERM, p );
		traverse_data->flags = f_set( INFORMED ); }
	return BM_DONE;
}

//===========================================================================
//	bm_couple
//===========================================================================
static listItem *
bm_couple( listItem *sub[2], BMContext *ctx )
/*
	Instantiates and/or returns all ( sub[0], sub[1] )
*/
{
	CNDB *db = BMContextDB( ctx );
	listItem *results = NULL, *s = NULL;
	if ( sub[0]->ptr == NULL ) {
		if ( sub[1]->ptr == NULL ) {
			listItem *t = NULL;
			for ( CNInstance *e=db_first(db,&s); e!=NULL; e=db_next(db,e,&s) )
			for ( CNInstance *f=db_first(db,&t); f!=NULL; f=db_next(db,f,&t) )
				addIfNotThere( &results, db_instantiate(e,f,db) );
		}
		else {
			for ( CNInstance *e=db_first(db,&s); e!=NULL; e=db_next(db,e,&s) )
			for ( listItem *j=sub[1]; j!=NULL; j=j->next )
				addIfNotThere( &results, db_instantiate(e,j->ptr,db) );
		}
	}
	else if ( sub[1]->ptr == NULL ) {
		listItem *t = NULL;
		for ( listItem *i=sub[0]; i!=NULL; i=i->next )
		for ( CNInstance *f=db_first(db,&t); f!=NULL; f=db_next(db,f,&t) )
			addIfNotThere( &results, db_instantiate(i->ptr,f,db) );
	}
	else {
		for ( listItem *i=sub[0]; i!=NULL; i=i->next )
		for ( listItem *j=sub[1]; j!=NULL; j=j->next )
			addIfNotThere( &results, db_instantiate(i->ptr,j->ptr,db) );
	}
	return results;
}

//===========================================================================
//	bm_literal
//===========================================================================
static CNInstance *
bm_literal( char *expression, BMContext *ctx )
/*
	Assumption: expression = (:ab...yz) resp. (:ab...z:)
	converts into (a,(b,...(y,z))) resp. (a,(b,...(z,'\0')))
*/
{
	CNDB *db = BMContextDB( ctx );
	listItem *stack = NULL;
	CNInstance *e=NULL, *mod=NULL, *backslash=NULL, *f;
	char *p = expression + 2;
	char_s q;
	for ( ; ; ) {
		switch ( *p ) {
		case ')':
			e = popListItem(&stack);
			while (( f = popListItem(&stack) ))
				e = db_instantiate( f, e, db );
			return e;
		case '%':
			if ( mod == NULL ) mod = db_register( "%", db, 0 );
			switch ( p[1] ) {
			case '%':
				e = db_instantiate( mod, mod, db );
				p+=2; break;
			default:
				p++;
				e = db_register( p, db, 0 );
				e = db_instantiate( mod, e, db );
				if ( !is_separator(*p) ) {
					p = p_prune( PRUNE_IDENTIFIER, p+1 );
					if ( *p=='\'' ) p++;
				}
			}
			addItem( &stack, e );
			break;
		case ':':
			if ( p[1]==')' )
				e = db_register( "\0", db, 0 );
			else {
				q.value = p[ 0 ];
				e = db_register( q.s, db, 0 );
			}
			addItem( &stack, e );
			p++; break;
		case '\\':
			switch (( q.value = p[1] )) {
			case '0':
			case ' ':
			case 'w':
				if ( backslash == NULL ) {
					backslash = db_register( "\\", db, 0 );
				}
				e = db_register( q.s, db, 0 );
				e = db_instantiate( backslash, e, db );
				p+=2; break;
			case ')':
				e = db_register( q.s, db, 0 );
				p+=2; break;
			default:
				p += charscan( p, &q );
				e = db_register( q.s, db, 0 );
			}
			addItem( &stack, e );
			break;
		default:
			q.value = p[0];
			e = db_register( q.s, db, 0 );
			addItem( &stack, e );
			p++;
		}
	}
}

//===========================================================================
//	bm_scan
//===========================================================================
static BMQueryCB scan_CB;

static listItem *
bm_scan( char *expression, BMContext *ctx )
{
	listItem *results = NULL;
	bm_query( BM_CONDITION, expression, ctx, scan_CB, &results );
	return results;
}
static BMCB_
scan_CB( CNInstance *e, BMContext *ctx, void *results )
{
	addIfNotThere((listItem **) results, e );
	return BM_CONTINUE;
}

