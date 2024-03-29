#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "traverse.h"
#include "proxy.h"
#include "cell.h"
#include "eenov.h"

// #define DEBUG

//===========================================================================
//	bm_proxy_scan
//===========================================================================
// #define TYPED

#ifdef TYPED
#define VERIFY( e, p, data ) \
	bm_verify( e, p, data )==BM_DONE
#define TYPESET( type ) \
	data.type = type; \
	data.privy = ( type==BM_RELEASED ? 1 : 0 );
#else
#define VERIFY( e, p, data ) \
	xp_verify( e, p, data )
#define TYPESET( type )
#endif

listItem *
bm_proxy_scan( BMQueryType type, char *expression, BMContext *ctx )
/*
	return all context's active connections (proxies) matching expression
*/
{
#ifdef DEBUG
	fprintf( stderr, "bm_proxy_scan: bgn - %s\n", expression );
#endif
	listItem *results = NULL;
	// Special case: expression is or includes %%
	if ( !strncmp( expression, "%%", 2 ) ) {
		CNInstance *proxy = BMContextSelf(ctx);
		results = newItem( proxy );
		return results; }
	else if ( *expression=='{' ) {
		char *p = expression;
		do {	p++;
			if ( !strncmp( p, "%%", 2 ) ) {
				CNInstance *proxy = BMContextSelf(ctx);
				results = newItem( proxy );
				break; }
			p = p_prune( PRUNE_TERM, p ); }
		while ( *p!='}' ); }
	// General case
	CNDB *db = BMContextDB( ctx );
	BMQueryData data;
	memset( &data, 0, sizeof(BMQueryData) );
	TYPESET( type );
	data.ctx = ctx;
	data.db = db;
	ActiveRV *active = BMContextActive( ctx );
	if ( *expression=='{' ) {
		for ( listItem *i=active->value; i!=NULL; i=i->next ) {
			CNInstance *proxy = i->ptr;
			char *p = expression;
			do {	p++;
				if ( !strncmp( p, "%%", 2 ) )
					{ p+=2; continue; }
				if ( VERIFY( proxy, p, &data ) ) {
					addIfNotThere( &results, proxy );
					break; }
				p = p_prune( PRUNE_TERM, p ); }
			while ( *p!='}' ); } }
	else {
		for ( listItem *i=active->value; i!=NULL; i=i->next ) {
			CNInstance *proxy = i->ptr;
			if ( VERIFY( proxy, expression, &data ) )
				addIfNotThere( &results, proxy ); } }
#ifdef DEBUG
	fprintf( stderr, "bm_proxy_scan: end\n" );
#endif
	return results;
}

//===========================================================================
//	bm_proxy_active / bm_proxy_in / bm_proxy_out
//===========================================================================
int
bm_proxy_active( CNInstance *proxy )
{
	CNEntity *cell = DBProxyThat( proxy );
	BMContext *ctx = BMCellContext( cell );
	CNDB *db = BMContextDB( ctx );
	return DBActive( db );
}
int
bm_proxy_in( CNInstance *proxy )
{
	CNEntity *cell = DBProxyThat( proxy );
	BMContext *ctx = BMCellContext( cell );
	CNDB *db = BMContextDB( ctx );
	return DBInitOn( db );
}
int
bm_proxy_out( CNInstance *proxy )
{
	CNEntity *cell = DBProxyThat( proxy );
	return ((cell) && *BMCellCarry(cell)==(void *)cell );
}

//===========================================================================
//	bm_proxy_feel
//===========================================================================
#include "proxy_traversal.h"

static CNInstance * proxy_feel_assignment( CNInstance *, char *, BMTraverseData * );
typedef struct {
	BMContext *ctx;
	struct { listItem *flags, *x; } stack;
	CNDB *db_x;
	CNInstance *x;
} ProxyFeelData;

CNInstance *
bm_proxy_feel( CNInstance *proxy, BMQueryType type, char *expression, BMContext *ctx )
{
#ifdef DEBUG
	fprintf( stderr, "bm_proxy_feel: %s\n", expression );
#endif
	CNEntity *cell = DBProxyThat( proxy );
	CNDB *db_x = BMContextDB( BMCellContext(cell) );

	int privy = ( type==BM_RELEASED ? 1 : 0 );
        CNInstance *success = NULL, *e;

	ProxyFeelData data;
	memset( &data, 0, sizeof(data) );
	data.ctx = ctx;
	data.db_x = db_x;

	BMTraverseData traverse_data;
	traverse_data.user_data = &data;
	traverse_data.stack = &data.stack.flags;
	traverse_data.done = 0;
	if ( *expression==':' )
		success = proxy_feel_assignment( proxy, expression, &traverse_data );
	else {
		listItem *s = NULL;
		for ( e=db_log(1,privy,db_x,&s); e!=NULL; e=db_log(0,privy,db_x,&s) ) {
			data.x = e;
			proxy_feel_traversal( expression, &traverse_data, FIRST );
			if ( traverse_data.done==2 ) {
				freeListItem( &data.stack.flags );
				freeListItem( &data.stack.x );
				traverse_data.done = 0; }
			else {
				freeListItem( &s );
				success = e;
				break; } } }
#ifdef DEBUG
	fprintf( stderr, "bm_proxy_feel: success=%d\n", !!success );
#endif
	return success;
}

//---------------------------------------------------------------------------
//	proxy_feel_traversal
//---------------------------------------------------------------------------
static inline int x_match( CNDB *, CNInstance *, char *, BMContext * );
static BMCBTake proxy_verify_CB( CNInstance *, BMContext *, void * );
#define proxy_verify( p, data ) \
	bm_query( BM_CONDITION, p, data->ctx, proxy_verify_CB, data )

BMTraverseCBSwitch( proxy_feel_traversal )
case_( term_CB )
	if is_f( FILTERED ) {
		if ( !proxy_verify( p, data ) )
			_return( 2 )
		_prune( BM_PRUNE_TERM ) }
	_break
case_( verify_CB )
	if ( !proxy_verify( p, data ) )
		_return( 2 )
	_prune( BM_PRUNE_TERM )
case_( open_CB )
	if ( f_next & COUPLE ) {
		CNInstance *x = data->x;
		if (( CNSUB( x, 0 ) )) {
			addItem( &data->stack.x, x );
			data->x = x->sub[ 0 ]; }
		else _return( 2 ) }
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

static BMCBTake
proxy_verify_CB( CNInstance *e, BMContext *ctx, void *user_data )
{
	ProxyFeelData *data = user_data;
	if ( db_match( data->db_x, data->x, BMContextDB(ctx), e ) )
		return BM_DONE;
	return BM_CONTINUE;
}

//---------------------------------------------------------------------------
//	x_match
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
		return DBStarMatch( x, db_x );
	case '.':
		return ( p[1]=='.' ) ?
			db_match( db_x, x, CTX_DB, BMContextParent(ctx) ) :
			db_match( db_x, x, CTX_DB, BMContextPerso(ctx) );
	case '%':
		switch ( p[1] ) {
		case '?': return db_match( db_x, x, CTX_DB, bm_context_lookup(ctx,"?") );
		case '!': return db_match( db_x, x, CTX_DB, bm_context_lookup(ctx,"!") );
		case '%': return db_match( db_x, x, CTX_DB, BMContextSelf(ctx) );
		case '<': return eenov_match( ctx, p, db_x, x ); }
		return ( !x->sub[0] && *DBIdentifier(x,db_x)=='%' ); }

	if ( !x->sub[0] ) {
		char *identifier = DBIdentifier( x, db_x );
		char_s q;
		switch ( *p ) {
		case '/': return !strcomp( p, identifier, 2 );
		case '\'': return charscan(p+1,&q) && !strcomp( q.s, identifier, 1 );
		default: return !strcomp( p, identifier, 1 ); } }

	return 0; // not a base entity
}

//===========================================================================
//	proxy_feel_assignment
//===========================================================================
static CNInstance *
proxy_feel_assignment( CNInstance *proxy, char *expression, BMTraverseData *traverse_data )
{
	ProxyFeelData *data = traverse_data->user_data;
	CNDB *db_x = data->db_x;
	CNInstance *star = db_lookup( 2, "*", db_x );
	if ( !star ) return NULL;
	expression++; // skip leading ':'
	char *value = p_prune( PRUNE_FILTER, expression ) + 1;
	CNInstance *success = NULL, *e;
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
				if ( !removeIfThere( &warden, e ) )
					addItem( &candidates, e ); }
			else if ( f->sub[ 0 ]==star ) {
				if ( !removeIfThere( &candidates, f ) )
					addItem( &warden, f ); } }
		freeListItem( &warden );
		while (( e = popListItem( &candidates ) )) {
			data->x = e->sub[ 1 ];
			proxy_feel_traversal( expression, traverse_data, FIRST );
			if ( traverse_data->done==2 ) {
				freeListItem( &data->stack.flags );
				freeListItem( &data->stack.x );
				traverse_data->done = 0; }
			else {
				freeListItem( &candidates );
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
			proxy_feel_traversal( expression, traverse_data, FIRST );
			if ( traverse_data->done==2 ) {
				freeListItem( &data->stack.flags );
				freeListItem( &data->stack.x );
				traverse_data->done = 0;
				continue; }
			data->x = e->sub[ 1 ];
			traverse_data->done = 0;
			proxy_feel_traversal( value, traverse_data, FIRST );
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

