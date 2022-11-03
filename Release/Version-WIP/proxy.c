#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "program.h"
#include "traverse.h"
#include "proxy.h"
#include "proxy_traversal.h"

// #define DEBUG

//===========================================================================
//	bm_proxy_op
//===========================================================================
static BMQueryCB activate_CB, deactivate_CB;

void
bm_proxy_op( char *expression, BMContext *ctx )
{
	if ( !expression || !(*expression)) return;
	char *p = expression;
	BMQueryCB *op = ( *p=='@' ) ?
		activate_CB: deactivate_CB;
	p += 2;
	if ( *p!='{' )
		bm_query( BM_CONDITION, p, ctx, op, NULL );
	else do {
		p++; bm_query( BM_CONDITION, p, ctx, op, NULL );
		p = p_prune( PRUNE_TERM, p );
	} while ( *p!='}' );
}
static BMCBTake
activate_CB( CNInstance *e, BMContext *ctx, void *user_data )
{
	if ( !isProxy(e) || isProxySelf(e) )
		return BM_CONTINUE;
	ActiveRV *active = BMContextActive( ctx );
	addIfNotThere( &active->buffer->activated, e );
	return BM_CONTINUE;
}
static BMCBTake
deactivate_CB( CNInstance *e, BMContext *ctx, void *user_data )
{
	if ( !isProxy(e) || isProxySelf(e) )
		return BM_CONTINUE;
	ActiveRV *active = BMContextActive( ctx );
	addIfNotThere( &active->buffer->deactivated, e );
	return BM_CONTINUE;
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

	CNDB *db = BMContextDB( ctx );
	listItem *results = NULL;
	BMQueryData data;
	memset( &data, 0, sizeof(BMQueryData) );
	data.type = BM_CONDITION;
	data.ctx = ctx;
	data.db = db;

	for ( listItem *i=BMContextActive(ctx)->value; i!=NULL; i=i->next ) {
		CNInstance *proxy = i->ptr;
		if ( xp_verify( proxy, expression, &data ) )
			addItem( &results, proxy ); }
	return results;
}

//===========================================================================
//	bm_proxy_still / bm_proxy_in
//===========================================================================
int
bm_proxy_still( CNInstance *proxy )
{
	CNEntity *that = BMProxyThat( proxy );
	BMContext *ctx = BMThisContext( that );
	return db_still( BMContextDB(ctx) );
}
int
bm_proxy_in( CNInstance *proxy )
{
	CNEntity *that = BMProxyThat( proxy );
	BMContext *ctx = BMThisContext( that );
	return db_in( BMContextDB(ctx) );
}

//===========================================================================
//	bm_proxy_feel
//===========================================================================
static CNInstance * bm_proxy_feel_assignment( CNInstance *, char *, BMTraverseData * );
typedef struct {
	BMContext *ctx;
	struct { listItem *flags, *x; } stack;
	CNDB *db_x;
	CNInstance *x;
} BMProxyFeelData;

static BMTraversal bm_proxy_feel_traversal;

#define BMTermCB		term_CB
#define BMNotCB			verify_CB
#define BMDereferenceCB		verify_CB
#define BMSubExpressionCB	verify_CB
#define BMDotExpressionCB	verify_CB
#define BMDotIdentifierCB	verify_CB
#define BMOpenCB		open_CB
#define BMDecoupleCB		decouple_CB
#define BMCloseCB		close_CB
#define BMRegisterVariableCB	identifier_CB
#define BMModCharacterCB	identifier_CB
#define BMStarCharacterCB	identifier_CB
//#define BMRegexCB		identifier_CB
#define BMCharacterCB		identifier_CB
#define BMIdentifierCB		identifier_CB

CNInstance *
bm_proxy_feel( CNInstance *proxy, BMQueryType type, char *expression, BMContext *ctx )
{
	if ( !expression || !(*expression)) return NULL;
#ifdef DEBUG
	fprintf( stderr, "BM_PROXY_FEEL: %s\n", expression );
#endif
	CNEntity *that = BMProxyThat( proxy );
	CNDB *db_x = BMContextDB( BMThisContext(that) );
	int privy = ( type==BM_RELEASED ? 1 : 0 );
        CNInstance *success = NULL, *e;

	BMProxyFeelData data;
	memset( &data, 0, sizeof(data) );
	data.ctx = ctx;
	data.db_x = db_x;

	BMTraverseData traverse_data;
	traverse_data.user_data = &data;
	traverse_data.stack = &data.stack.flags;
	traverse_data.done = 0;
	if ( *expression==':' )
		success = bm_proxy_feel_assignment( proxy, expression, &traverse_data );
	else {
		listItem *s = NULL;
		for ( e=db_log(1,privy,db_x,&s); e!=NULL; e=db_log(0,privy,db_x,&s) ) {
			data.x = e;
			bm_proxy_feel_traversal( expression, &traverse_data, FIRST );
			if ( traverse_data.done==2 ) {
				freeListItem( &data.stack.flags );
				freeListItem( &data.stack.x );
				traverse_data.done = 0; }
			else {
				freeListItem( &s );
				success = e;
				break; } } }
#ifdef DEBUG
	fprintf( stderr, "BM_PROXY_FEEL: success=%d\n", !!success );
#endif
	return success;
}

//---------------------------------------------------------------------------
//	bm_proxy_feel_traversal
//---------------------------------------------------------------------------
#include "traversal.h"

static inline int x_match( CNDB *, CNInstance *, char *, BMContext * );
static BMCBTake proxy_verify_CB( CNInstance *, BMContext *, void * );
#define bm_proxy_verify( p, data ) \
	bm_query( BM_CONDITION, p, data->ctx, proxy_verify_CB, data )

BMTraverseCBSwitch( bm_proxy_feel_traversal )
case_( term_CB )
	if is_f( FILTERED ) {
		if ( !bm_proxy_verify( p, data ) )
			_return( 2 )
		_prune( BM_PRUNE_TERM ) }
	_break
case_( verify_CB )
	if ( !bm_proxy_verify( p, data ) )
		_return( 2 )
	_prune( BM_PRUNE_TERM )
case_( open_CB )
	if ( f_next & COUPLE ) {
		CNInstance *x = data->x;
		if ( !CNSUB(x,0) )
			_return( 2 )
		addItem( &data->stack.x, x );
		data->x = x->sub[ 0 ]; }
	_break
case_( decouple_CB )
	data->x = ((CNInstance *) data->stack.x->ptr )->sub[ 1 ];
	_break
case_( close_CB )
	if is_f( COUPLE )
		data->x = popListItem( &data->stack.x );
	_break
case_( identifier_CB )
	if ( !x_match( data->db_x, data->x, p, data->ctx ) )
		_return( 2 )
	_break
BMTraverseCBEnd

static inline int db_x_match( CNDB *, CNInstance *, CNDB *, CNInstance * );
static BMCBTake
proxy_verify_CB( CNInstance *e, BMContext *ctx, void *user_data )
{
	BMProxyFeelData *data = user_data;
	if ( db_x_match( data->db_x, data->x, BMContextDB(ctx), e ) )
		return BM_DONE;
	return BM_CONTINUE;
}

//---------------------------------------------------------------------------
//	x_match / db_x_match
//---------------------------------------------------------------------------
#define CTX_DB	BMContextDB(ctx)

static inline int
x_match( CNDB *db_x, CNInstance *x, char *p, BMContext *ctx )
/*
	matching is taking place in external event narrative occurrence
*/
{
	switch ( *p ) {
	case '*':
		return ( x==db_star(db_x) );
	case '.':
		return ( p[1]=='.' ) ?
			db_x_match( db_x, x, CTX_DB, BMContextParent(ctx) ) :
			db_x_match( db_x, x, CTX_DB, BMContextPerso(ctx) );
	case '%':
		switch ( p[1] ) {
		case '?': return db_x_match( db_x, x, CTX_DB, bm_context_lookup(ctx,"?") );
		case '!': return db_x_match( db_x, x, CTX_DB, bm_context_lookup(ctx,"!") );
		case '%': return db_x_match( db_x, x, CTX_DB, BMContextSelf(ctx) );
		case '<': return eeno_match( ctx, p, db_x, x ); }
		return ( !x->sub[0] && *db_identifier(x,db_x)=='%' ); }

	if ( !x->sub[0] ) {
		char *identifier = db_identifier( x, db_x );
		char_s q;
		switch ( *p ) {
		case '/': return !strcomp( p, identifier, 2 );
		case '\'': return charscan(p+1,&q) && !strcomp( q.s, identifier, 1 );
		default: return !strcomp( p, identifier, 1 ); } }

	return 0; // not a base entity
}
static inline int
db_x_match( CNDB *db_x, CNInstance *x, CNDB *db_y, CNInstance *y )
/*
	Assumption: x!=NULL
	Note that we use y to lead the traversal - but works either way
*/
{
	if ( !y ) return 0;
	listItem *stack = NULL;
	int ndx = 0;
	for ( ; ; ) {
		if (( CNSUB(y,ndx) )) {
			if ( !CNSUB(x,ndx) )
				goto FAIL;
			add_item( &stack, ndx );
			addItem( &stack, y );
			addItem( &stack, x );
			y = y->sub[ ndx ];
			x = x->sub[ ndx ];
			ndx = 0; continue; }

		if (( y->sub[ 0 ] )) {
			if ( !isProxy(x) || BMProxyThat(x)!=BMProxyThat(y) )
				goto FAIL; }
		else {
			if (( x->sub[ 0 ] ))
				goto FAIL;
			char *p_x = db_identifier( x, db_x );
			char *p_y = db_identifier( y, db_y );
			if ( strcomp( p_x, p_y, 1 ) )
				goto FAIL; }
		for ( ; ; ) {
			if ( !stack ) return 1;
			x = popListItem( &stack );
			y = popListItem( &stack );
			if ( !pop_item( &stack ) )
				{ ndx=1; break; }
		} }
FAIL:
	freeListItem( &stack );
	return 0;
}

//---------------------------------------------------------------------------
//	eeno_match / eeno_inform / eeno_lookup
//---------------------------------------------------------------------------
typedef struct {
	CNInstance *src;
	CNInstance *result;
} EEnoData;
static inline int eeno_read( BMContext *, char *, EEnoData * );

int
eeno_match( BMContext *ctx, char *p, CNDB *db_x, CNInstance *x )
/*
	Note that when invoked by query.c:match() then
		BMContextDB(ctx)==db_x
*/
{
	EEnoData data;
	switch ( eeno_read(ctx,p,&data) ) {
	case 0: return 0;
	case 1: return db_x_match( db_x, x, BMContextDB(ctx), data.src ); 
	default: ;
		CNEntity *that = BMProxyThat( data.src );
		CNDB *db_y = BMContextDB( BMThisContext(that) );
		return db_x_match( db_x, x, db_y, data.result ); }
}
CNInstance *
eeno_inform( BMContext *ctx, CNDB *db, char *p, BMContext *dst )
{
	EEnoData data;
	switch ( eeno_read(ctx,p,&data) ) {
	case 0: return NULL;
	case 1: return ((dst) ? bm_inform_context(db,data.src,dst) : data.src );
	default:
		db = BMContextDB( dst );
		return bm_inform_context( db, data.result, ((dst)?dst:ctx) ); }
}
CNInstance *
eeno_lookup( BMContext *ctx, CNDB *db, char *p )
{
	EEnoData data;
	switch ( eeno_read(ctx,p,&data) ) {
	case 0: return NULL;
	case 1: return data.src;
	default: ;
		CNEntity *that = BMProxyThat( data.src );
		CNDB *db_x = BMContextDB( BMThisContext(that) );
		return bm_lookup_x( db, data.result, ctx, db ); }
}

static inline int
eeno_read( BMContext *ctx, char *p, EEnoData *data )
{
	EEnoRV *eeno = bm_context_lookup( ctx, "<" );
	if ( !eeno ) return 0;
	data->src = eeno->src;

	p+=2; // skip leading "%<"
	CNInstance *y;
	switch ( *p++ ) {
	case '?': y = eeno->event->name; break;
	case '!': y = eeno->event->value; break;
	default: return 1; }

	if ( *p==':' ) {
		listItem *stack = NULL;
		CNInstance *sub = y;
		int position = 0;
		for ( p++; *p!='>'; p++ )
			switch ( *p ) {
			case '(':
				addItem( &stack, sub );
				add_item( &stack, position );
				if ( !p_single(p) ) {
					sub = CNSUB( sub, position );
					if ( !sub ) {
						freeListItem( &stack );
						return 0; } }
				position = 0;
				break;
			case '?': 
				y = sub;
				break;
			case ',':
				sub = ((CNInstance*)stack->ptr)->sub[ 1 ];
				position = 1;
				break;
			case ')':
				position = pop_item( &stack );
				sub = popListItem( &stack );
				break; } }
	data->result = y;
	return 2;
}

//===========================================================================
//	bm_proxy_feel_assignment
//===========================================================================
static CNInstance *
bm_proxy_feel_assignment( CNInstance *proxy, char *expression, BMTraverseData *traverse_data )
{
	BMProxyFeelData *data = traverse_data->user_data;
	expression++; // skip leading ':'
	char *value = p_prune( PRUNE_FILTER, expression ) + 1;
	CNDB *db_x = data->db_x;
	CNInstance *success = NULL, *e;
	CNInstance *star = db_star( db_x );
	if ( !strncmp( value, "~.", 2 ) ) {
		/* we want to find x:(*,variable) in manifested log, so that
		   	. ( x, . ) is not manifested as well
		   	. variable verifies expression
		   There is no guarantee that, were both ((*,variable),.) and
		   (*,variable) manifested, they would appear in any order.
		   So first we generate a list of suitable candidates...
		   Assumption: if they do appear, they appear only once.
		*/
		listItem *s = NULL;
		listItem *warden=NULL, *candidates=NULL;
		for ( e=db_log(1,0,db_x,&s); e!=NULL; e=db_log(0,0,db_x,&s) ) {
			CNInstance *f = CNSUB( e, 0 );
			if ( !f ) continue;
			else if ( f==star ) {
				if ( !removeIfThere( &warden, f ) )
					addItem( &candidates, f ); }
			else if ( f->sub[ 0 ]==star ) {
				if ( !removeIfThere( &candidates, f ) )
					addItem( &warden, f ); } }
		freeListItem( &warden );
		while (( e = popListItem( &candidates ) )) {
			data->x = e->sub[ 0 ];
			bm_proxy_feel_traversal( expression, traverse_data, FIRST );
			if ( traverse_data->done==2 ) {
				freeListItem( &data->stack.flags );
				freeListItem( &data->stack.x );
				traverse_data->done = 0; }
			else {
				freeListItem( &s );
				success = e; // return (*,.)
				break; } } }
	else {
		/* we want to find x:((*,variable),value) in manifested log,
		   so that:
			. variable verifies expression
			. value verifies value expression
		*/
		listItem *s = NULL;
		for ( e=db_log(1,0,db_x,&s); e!=NULL; e=db_log(0,0,db_x,&s) ) {
			CNInstance *f = CNSUB( e, 0 ); // e:( f, . )
			if ( !f || f->sub[0]!=star ) continue;
			data->x = f->sub[ 1 ];
			bm_proxy_feel_traversal( expression, traverse_data, FIRST );
			if ( traverse_data->done==2 ) {
				freeListItem( &data->stack.flags );
				freeListItem( &data->stack.x );
				traverse_data->done = 0;
				continue; }
			data->x = e->sub[ 1 ];
			traverse_data->done = 0;
			bm_proxy_feel_traversal( value, traverse_data, FIRST );
			if ( traverse_data->done==2 ) {
				freeListItem( &data->stack.flags );
				freeListItem( &data->stack.x );
				traverse_data->done = 0; }
			else {
				freeListItem( &s );
				success = e; // return ((*,.),.)
				break; } } }
	return success;
}

