#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "traverse.h"
#include "proxy.h"
#include "cell.h"
#include "eenov.h"

// #define DEBUG

//===========================================================================
//	bm_proxy_feel
//===========================================================================
#include "proxy_traversal.h"

static void * proxy_feel_assignment( CNInstance *, int, char *, BMTraverseData * );
typedef struct {
	BMContext *ctx;
	CNDB *db_x;
	CNInstance *x;
	struct { listItem *flags, *x; } stack;
} ProxyFeelData;

void *
bm_proxy_feel( CNInstance *proxy, int type, char *expression, BMContext *ctx )
{
#ifdef DEBUG
	fprintf( stderr, "bm_proxy_feel: %s\n", expression );
#endif
	CNEntity *cell = DBProxyThat( proxy );
	CNDB *db_x = BMContextDB( BMCellContext(cell) );

	int as_per = ( type & BM_AS_PER );
	int privy = !( type & BM_VISIBLE );
        void *success = NULL;
        CNInstance *e;

	ProxyFeelData data;
	memset( &data, 0, sizeof(data) );
	data.ctx = ctx;
	data.db_x = db_x;

	BMTraverseData traverse_data;
	traverse_data.user_data = &data;
	traverse_data.stack = &data.stack.flags;
	traverse_data.done = 0;
	if ( *expression==':' )
		success = proxy_feel_assignment( proxy, as_per, expression, &traverse_data );
	else {
		listItem *s = NULL;
		for ( e=DBLog(1,privy,db_x,&s); e!=NULL; e=DBLog(0,privy,db_x,&s) ) {
			data.x = e;
			proxy_feel_traversal( expression, &traverse_data, FIRST );
			if ( traverse_data.done==2 ) {
				freeListItem( &data.stack.flags );
				freeListItem( &data.stack.x );
				traverse_data.done = 0; }
			else if ( as_per ) {
				addItem((listItem **) &success, e ); }
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
static inline CNInstance * proxy_verify( char *p, ProxyFeelData *data );

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
	BMContext *ctx = data->ctx;
	CNDB *db = BMContextDB( ctx );
	if ( !bm_match( ctx, db, p, data->x, data->db_x ) )
		_return( 2 )
	_break
BMTraverseCBEnd

static BMCBTake proxy_verify_CB( CNInstance *, BMContext *, void * );
static inline CNInstance *
proxy_verify( char *p, ProxyFeelData *data )
{
	return bm_query( BM_CONDITION, p, data->ctx, proxy_verify_CB, data );
}
static BMCBTake
proxy_verify_CB( CNInstance *e, BMContext *ctx, void *user_data )
{
	ProxyFeelData *data = user_data;
	if ( db_match( data->x, data->db_x, e, BMContextDB(ctx) ) )
		return BM_DONE;
	return BM_CONTINUE;
}

//---------------------------------------------------------------------------
//	proxy_feel_assignment
//---------------------------------------------------------------------------
static void *
proxy_feel_assignment( CNInstance *proxy, int as_per, char *expression, BMTraverseData *traverse_data )
{
	ProxyFeelData *data = traverse_data->user_data;
	CNDB *db_x = data->db_x;
	CNInstance *star = db_lookup( 2, "*", db_x );
	if ( !star ) return NULL;
	expression++; // skip leading ':'
	char *value = p_prune( PRUNE_FILTER, expression ) + 1;
	void *success = NULL;
	CNInstance *e;
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
		for ( e=DBLog(1,0,db_x,&s); e!=NULL; e=DBLog(0,0,db_x,&s) ) {
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
			else if ( as_per ) {
				addItem((listItem **) &success, e ); }
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
		for ( e=DBLog(1,0,db_x,&s); e!=NULL; e=DBLog(0,0,db_x,&s) ) {
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
			else if ( as_per ) {
				addItem((listItem **) &success, e ); }
			else {
				freeListItem( &s );
				success = e; // return ((*,.),.)
				break; } } }
	return success;
}

//===========================================================================
//	bm_proxy_scan
//===========================================================================
listItem *
bm_proxy_scan( int type, char *expression, BMContext *ctx )
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
				if ( xp_verify( proxy, p, &data ) ) {
					addIfNotThere( &results, proxy );
					break; }
				p = p_prune( PRUNE_TERM, p ); }
			while ( *p!='}' ); } }
	else {
		for ( listItem *i=active->value; i!=NULL; i=i->next ) {
			CNInstance *proxy = i->ptr;
			if ( xp_verify( proxy, expression, &data ) )
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

