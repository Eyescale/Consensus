#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "traverse.h"
#include "cell.h"
#include "eenov.h"

//===========================================================================
//	eenov_output / eenov_inform / eenov_lookup / eenov_match
//===========================================================================
typedef struct {
	CNInstance *src;
	CNInstance *result;
	struct { listItem *instance, *flags; } stack;
	CNInstance *instance;
	int success;
	CNDB *db;
} EEnovData;
static inline int eenov_read( BMContext *, char *, EEnovData * );

int
eenov_output( BMContext *ctx, int type, char *p )
{
	EEnovData data;
	switch ( eenov_read(ctx,p,&data) ) {
	case 0: break;
	case 1: ;
		CNDB *db = BMContextDB( ctx );
		db_outputf( stdout, db, (type=='s'?"%s":"%_"), data.src );
		break;
	default: ;
		CNEntity *cell = DBProxyThat( data.src );
		CNDB *db_x = BMContextDB( BMCellContext(cell) );
		db_outputf( stdout, db_x, (type=='s'?"%s":"%_"), data.result ); }
	return 0;
}
CNInstance *
eenov_inform( BMContext *ctx, CNDB *db, char *p, BMContext *dst )
{
	EEnovData data;
	switch ( eenov_read(ctx,p,&data) ) {
	case 0: return NULL;
	case 1: return ((dst) ? bm_inform_context(db,data.src,dst) : data.src );
	default: ;
		CNEntity *cell = DBProxyThat( data.src );
		CNDB *db_x = BMContextDB( BMCellContext(cell) );
		return bm_inform_context( db_x, data.result, ((dst)?dst:ctx) ); }
}
CNInstance *
eenov_lookup( BMContext *ctx, CNDB *db, char *p )
{
	EEnovData data;
	switch ( eenov_read(ctx,p,&data) ) {
	case 0: return NULL;
	case 1: return data.src;
	default: ;
		if ( !db ) return data.result;
		CNEntity *cell = DBProxyThat( data.src );
		CNDB *db_x = BMContextDB( BMCellContext(cell) );
		return bm_lookup_x( db_x, data.result, ctx, db ); }
}
int
eenov_match( BMContext *ctx, char *p, CNDB *db_x, CNInstance *x )
/*
	Note that when invoked by query.c:match() then
		BMContextDB(ctx)==db_x
*/
{
	EEnovData data;
	switch ( eenov_read(ctx,p,&data) ) {
	case 0: return 0;
	case 1: return db_match( db_x, x, BMContextDB(ctx), data.src ); 
	default: ;
		CNEntity *cell = DBProxyThat( data.src );
		CNDB *db_y = BMContextDB( BMCellContext(cell) );
		return db_match( db_x, x, db_y, data.result ); }
}

//---------------------------------------------------------------------------
//	eenov_read
//---------------------------------------------------------------------------
#include "eenov_traversal.h"

static inline int
eenov_read( BMContext *ctx, char *p, EEnovData *data )
{
	EEnoRV *eenov = bm_context_lookup( ctx, "<" );
	if ( !eenov ) return 0;
	data->src = eenov->src;

	p+=2; // skip leading "%<"
	CNInstance *y;
	switch ( *p++ ) {
	case '?': y = eenov->event->name; break;
	case '!': y = eenov->event->value; break;
	default: return 1; }

	if ( *p++==':' ) {
		CNEntity *cell = DBProxyThat( data->src );
		data->db = BMContextDB( BMCellContext(cell) );
		data->result = y;
		data->instance = y;
		data->stack.flags = NULL;
		data->stack.instance = NULL;

		BMTraverseData traverse_data;
		traverse_data.user_data = data;
		traverse_data.stack = &data->stack.flags;
		traverse_data.done = 0;

		eenov_traversal( p, &traverse_data, FIRST );

		return ( data->success ? 2 : 0 ); }
	else {
		data->result = y;
		return 2; }
}

//---------------------------------------------------------------------------
//	eenov_traversal
//---------------------------------------------------------------------------
BMTraverseCBSwitch( eenov_traversal )
case_( identifier_CB )
	CNInstance *x = data->instance;
	int success = 0;
	if ( !x->sub[0] ) {
		char *identifier = db_identifier( x, data->db );
		char_s q;
		switch ( *p ) {
		case '/': success = !strcomp( p, identifier, 2 ); break;
		case '\'': success = charscan(p+1,&q) && !strcomp( q.s, identifier, 1 ); break;
		default: success = !strcomp( p, identifier, 1 ); } }
	data->success = is_f( NEGATED ) ? !success : success;
	_break
case_( open_CB )
	if ( f_next & COUPLE ) {
		CNInstance *x = data->instance;
		x = CNSUB( x, is_f(FIRST)?0:1 );
		data->success = is_f( NEGATED ) ? !x : !!x;
		if ( data->success ) {
			addItem( &data->stack.instance, data->instance );
			data->instance = x; }
		else _prune( BM_PRUNE_TERM ) }
	_break
case_( decouple_CB )
	if ( !data->success )
		_prune( BM_PRUNE_TERM )
	else {
		listItem *stack = data->stack.instance;
		data->instance = ((CNInstance*)stack->ptr)->sub[ 1 ];
		_break }
case_( close_CB )
	if ( is_f(COUPLE) )
		data->instance = popListItem( &data->stack.instance );
	if ( f_next & NEGATED ) data->success = !data->success;
	_break
case_( wildcard_CB )
	if ( *p=='?' )
		data->result = data->instance;
	data->success = !is_f( NEGATED );
	_break
case_( end_CB )
	_return( 1 )
BMTraverseCBEnd
