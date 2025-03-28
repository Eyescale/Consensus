#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "eenov.h"
#include "cell.h"
#include "errout.h"

// #define DEBUG

#include "eeno_query.h"
#include "eeno_query_traversal.h"

//===========================================================================
//	bm_eeno_query
//===========================================================================
static void * eeno_query_assignment( CNInstance *, int, char *, BMTraverseData * );
typedef struct {
	BMContext *ctx;
	CNDB *db_x;
	CNInstance *x;
	struct { listItem *flags, *x; } stack;
	} EENOFeelData;

void *
bm_eeno_query( int type, char *expression, CNInstance *proxy, BMContext *ctx ) {
#ifdef DEBUG
	fprintf( stderr, "bm_eeno_query: %s\n", expression );
#endif
	CNEntity *cell = CNProxyThat( proxy );
	CNDB *db_x = BMContextDB( BMCellContext(cell) );

	int as_per = ( type & BM_AS_PER );
	int privy = !( type & BM_VISIBLE );
        void *success = NULL;
        CNInstance *e;

	EENOFeelData data;
	memset( &data, 0, sizeof(data) );
	data.ctx = ctx;
	data.db_x = db_x;

	BMTraverseData traversal;
	traversal.user_data = &data;
	traversal.stack = &data.stack.flags;
	traversal.done = 0;
	if ( *expression==':' ) {
		success = eeno_query_assignment( proxy, as_per, expression, &traversal ); }
	else {
		listItem *s = NULL;
		for ( e=DBLog(1,privy,db_x,&s); e!=NULL; e=DBLog(0,privy,db_x,&s) ) {
			data.x = e;
			eeno_query_traversal( expression, &traversal, FIRST );
			if ( traversal.done==2 ) {
				freeListItem( &data.stack.flags );
				freeListItem( &data.stack.x );
				traversal.done = 0; }
			else if ( as_per ) {
				addItem((listItem **) &success, e ); }
			else {
				freeListItem( &s );
				success = e;
				break; } } }
#ifdef DEBUG
	fprintf( stderr, "bm_eeno_query: success=%d\n", !!success );
#endif
	return success; }

//---------------------------------------------------------------------------
//	eeno_query_traversal
//---------------------------------------------------------------------------
static inline CNInstance * proxy_verify( char *, EENOFeelData * );

BMTraverseCBSwitch( eeno_query_traversal )
case_( filter_CB )
	errout( EENOQueryFiltered, p );
	_prune( BMCB_TERM, p+1 )
case_( verify_CB )
	if ( !proxy_verify( p, data ) )
		_return( 2 )
	_prune( BMCB_TERM, p )
case_( open_CB )
	if is_f_next( COUPLE ) {
		CNInstance *x = data->x;
		if (( CNSUB( x, 0 ) )) {
			addItem( &data->stack.x, x );
			data->x = x->sub[ 0 ]; }
		else _return( 2 ) }
	_break
case_( comma_CB )
	CNInstance *x = data->stack.x->ptr;
	data->x = x->sub[ 1 ];
	_break
case_( close_CB )
	if is_f( COUPLE )
		data->x = popListItem( &data->stack.x );
	_break
case_( match_CB )
	BMContext *ctx = data->ctx;
	CNDB *db = BMContextDB( ctx );
	if ( !bm_match( ctx, db, p, data->x, data->db_x ) )
		_return( 2 )
	_break
case_( wildcard_CB )
	if ( !strncmp( p, "?:", 2 ) )
		_continue( p+2 )
	_break
BMTraverseCBEnd

static BMQueryCB proxy_verify_CB;
static inline CNInstance *
proxy_verify( char *p, EENOFeelData *data ) {
	return bm_query( BM_CONDITION, p, data->ctx, proxy_verify_CB, data ); }

static BMQTake
proxy_verify_CB( CNInstance *e, BMContext *ctx, void *user_data ) {
	EENOFeelData *data = user_data;
	if ( db_match( data->x, data->db_x, e, BMContextDB(ctx) ) )
		return BMQ_DONE;
	return BMQ_CONTINUE; }

//---------------------------------------------------------------------------
//	eeno_query_assignment
//---------------------------------------------------------------------------
static void *
eeno_query_assignment( CNInstance *proxy, int as_per, char *expression, BMTraverseData *traversal ) {
	EENOFeelData *data = traversal->user_data;
	CNDB *db_x = data->db_x;
	CNInstance *star = db_lookup( 2, "*", db_x );
	if ( !star ) return NULL;
	CNInstance *success = NULL, *e, *f;

	expression++; // skip leading ':'
	if ( !strcmp(expression,"~.") ) {
		/* we want to find x:(*,proxy_self) in manifested log, so that
			we have proxy_self:((NULL,that),NULL) where proxy:((this,that),NULL)
			but we don't have ( x, . ) in there as well
		*/
		listItem *s = NULL;
		CNInstance *that = CNProxyThat( proxy );
		for ( e=DBLog(1,0,db_x,&s); e!=NULL; e=DBLog(0,0,db_x,&s) ) {
			if (( success )) {
				if ( e->sub[ 0 ]==success ) {
					success = NULL;
					freeListItem( &s );
					break; } }
			else if ( e->sub[0]==star ) {
				f = e->sub[ 1 ];
				if (( f->sub[0] ) && CNProxyThat(f)==that )
					success = f; }
			else if (( f=CNSUB(e,0) ) && f->sub[0]==star ) {
				f = f->sub[ 1 ]; // here we have e:((*,f),.)
				if (( f->sub[0] ) && CNProxyThat(f)==that ) {
					freeListItem( &s );
					break; } } }
		return success; } // (*,%%) manifested

	char *value = p_prune( PRUNE_FILTER, expression );
	if ( *value++=='\0' ) { // moving past ',' aka. ':' if there is
		/* we want to find x:((*,proxy_self),expression) in manifested log, so that
			proxy_self:((NULL,that),NULL) where proxy:((this,that),NULL)
		*/
		listItem *s = NULL;
		CNInstance *that = CNProxyThat( proxy );
		for ( e=DBLog(1,0,db_x,&s); e!=NULL; e=DBLog(0,0,db_x,&s) ) {
			CNInstance *f = CNSUB( e, 0 ); // e:( f, . )
			if ( !f || f->sub[0]!=star ) continue;
			CNInstance *g = f->sub[ 1 ];
			if ( !g->sub[0] || CNProxyThat(g)!=that )
				continue;
			// here we have g:((.,that),.)=>=proxy-Self
			data->x = e->sub[ 1 ];
			eeno_query_traversal( expression, traversal, FIRST );
			if ( traversal->done==2 ) {
				freeListItem( &data->stack.flags );
				freeListItem( &data->stack.x );
				traversal->done = 0; }
			else {
				freeListItem( &s );
				success = e; // return ((*,.),.)
				break; } } }
	else if ( !strncmp( value, "~.", 2 ) ) {
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
			eeno_query_traversal( expression, traversal, FIRST );
			if ( traversal->done==2 ) {
				freeListItem( &data->stack.flags );
				freeListItem( &data->stack.x );
				traversal->done = 0; }
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
			eeno_query_traversal( expression, traversal, FIRST );
			if ( traversal->done==2 ) {
				freeListItem( &data->stack.flags );
				freeListItem( &data->stack.x );
				traversal->done = 0;
				continue; }
			data->x = e->sub[ 1 ];
			traversal->done = 0;
			eeno_query_traversal( value, traversal, FIRST );
			if ( traversal->done==2 ) {
				freeListItem( &data->stack.flags );
				freeListItem( &data->stack.x );
				traversal->done = 0; }
			else if ( as_per ) {
				addItem((listItem **) &success, e ); }
			else {
				freeListItem( &s );
				success = e; // return ((*,.),.)
				break; } } }
	return success; }

//===========================================================================
//	bm_eeno_scan
//===========================================================================
listItem *
bm_eeno_scan( char *src, BMContext *ctx )
/*
	return all context's active connections (proxies) matching expression
*/ {
#ifdef DEBUG
	fprintf( stderr, "bm_eeno_scan: bgn - %s\n", src );
#endif
	listItem *results = NULL;
	if ( *src=='?' ) {
		if ( src[1]==':' ) src+=2; // skip leading "?:"
		else {
			ActiveRV *active = BMContextActive( ctx );
			for ( listItem *i=active->value; i!=NULL; i=i->next )
				addItem( &results, i->ptr );
			reorderListItem( &results );
			return results; } }

	// Special case: src is or includes %%
	if ( !strncmp( src, "%%", 2 ) ) {
		CNInstance *proxy = BMContextSelf(ctx);
		results = newItem( proxy );
		return results; }
	else if ( *src=='{' ) {
		char *p = src;
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
	if ( *src=='{' ) {
		for ( listItem *i=active->value; i!=NULL; i=i->next ) {
			CNInstance *proxy = i->ptr;
			char *p = src;
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
			if ( xp_verify( proxy, src, &data ) )
				addIfNotThere( &results, proxy ); } }
#ifdef DEBUG
	fprintf( stderr, "bm_eeno_scan: end\n" );
#endif
	return results; }

//===========================================================================
//	bm_proxy_active / bm_proxy_in / bm_proxy_out
//===========================================================================
int
bm_proxy_active( CNInstance *proxy ) {
	CNEntity *cell = CNProxyThat( proxy );
	BMContext *ctx = BMCellContext( cell );
	CNDB *db = BMContextDB( ctx );
	return DBActive( db ); }

int
bm_proxy_in( CNInstance *proxy ) {
	CNEntity *cell = CNProxyThat( proxy );
	BMContext *ctx = BMCellContext( cell );
	CNDB *db = BMContextDB( ctx );
	return DBInitOn( db ); }

int
bm_proxy_out( CNInstance *proxy ) {
	CNEntity *cell = CNProxyThat( proxy );
	return ((cell) && bm_cell_out(cell) ); }

