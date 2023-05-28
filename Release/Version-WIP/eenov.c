#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "eenov.h"
#include "eenov_private.h"
#include "locate_emark.h"
#include "traverse.h"
#include "cell.h"

//===========================================================================
//	eenov_output / eenov_inform / eenov_lookup / eenov_match
//===========================================================================
static EEnovType eenov_type( BMContext *, char *, EEnovData * );
static CNInstance * eenov_query_op( EEnovQueryOp, BMContext *, char *, EEnovData * );

int
eenov_output( char *p, BMContext *ctx, OutputData *od )
{
	EEnovData data;
	switch ( eenov_type(ctx,p,&data) ) {
	case EEnovNone:
		return 0;
	case EEnovSrcType: ;
		CNDB *db = BMContextDB( ctx );
		char *fmt = (od->type=='s'?"%s":"%_");
		return db_outputf( od->stream, db, fmt, data.src );
	case EEnovInstanceType: ;
		fmt = (od->type=='s'?"%s":"%_");
		return db_outputf( od->stream, data.db, fmt, data.instance );
	case EEnovExprType:
		data.param.output.od = od;
		eenov_query_op( EEnovOutputOp, ctx, p+2, &data );
		return bm_out_flush( od, data.db ); }
}
listItem *
eenov_inform( BMContext *ctx, CNDB *db, char *p, BMContext *dst )
{
	EEnovData data;
	switch ( eenov_type(ctx,p,&data) ) {
	case EEnovNone:
		return NULL;
	case EEnovSrcType: ;
		CNInstance *e = ((dst) ? bm_inform(0,dst,data.src,db) : data.src );
		return ((e) ? newItem(e) : NULL );
	case EEnovInstanceType:
		e = bm_inform( 0, ((dst)?dst:ctx), data.instance, data.db );
		return ((e) ? newItem(e) : NULL );
	case EEnovExprType:
		data.param.inform.ctx = ((dst)?dst:ctx);
		data.param.inform.result = NULL;
		eenov_query_op( EEnovInformOp, ctx, p+2, &data );
		reorderListItem( &data.param.inform.result );
		return data.param.inform.result; }
}
CNInstance *
eenov_lookup( BMContext *ctx, CNDB *db, char *p )
/*
	Note that when db==NULL we return data.instance as is
*/
{
	EEnovData data;
	switch ( eenov_type(ctx,p,&data) ) {
	case EEnovNone:
		return NULL;
	case EEnovSrcType:
		return data.src;
	case EEnovInstanceType:
		if ( !db ) return data.instance;
		return bm_intake( ctx, db, data.instance, data.db );
	case EEnovExprType:
		data.param.lookup.ctx = ctx;
		data.param.lookup.db = db;
		return eenov_query_op( EEnovLookupOp, ctx, p+2, &data ); }
}
int
eenov_match( BMContext *ctx, char *p, CNInstance *x, CNDB *db_x )
/*
	Note that when invoked via query.c:match() then
		BMContextDB(ctx)==data.db==db_x
*/
{
	EEnovData data;
	switch ( eenov_type(ctx,p,&data) ) {
	case EEnovNone:
		return 0;
	case EEnovSrcType:
		return db_match( x, db_x, data.src, BMContextDB(ctx) ); 
	case EEnovInstanceType:
		return db_match( x, db_x, data.instance, data.db );
	case EEnovExprType:
		data.param.match.x = x;
		data.param.match.db = db_x;
		return !!eenov_query_op( EEnovMatchOp, ctx, p+2, &data ); }
}

//===========================================================================
//	eenov_type
//===========================================================================
#include "eenov_traversal.h"

static EEnovType
eenov_type( BMContext *ctx, char *p, EEnovData *data )
{
	EEnoRV *eenov = BMContextEENOVCurrent( ctx );
	if (( eenov )) {
		CNInstance *src = eenov->src;
		data->src = src;

		switch ( p[2] ) {
		case '!': data->instance = eenov->event->name; break;
		case '?': data->instance = eenov->event->value; break;
		case '(': data->instance = eenov->event->name; break;
		default: return EEnovSrcType; }

		CNEntity *cell = DBProxyThat( src );
		data->db = BMContextDB( BMCellContext(cell) );
		if ( p[2]=='(' )
			return EEnovExprType;
		else if ( p[3]=='>' )
			return EEnovInstanceType;
		else {

			BMTraverseData traverse_data;
			traverse_data.user_data = data;
			traverse_data.stack = &data->stack.flags;

			data->stack.flags = NULL;
			data->stack.instance = NULL;
			data->result = data->instance;
			traverse_data.done = 0;
			p = eenov_traversal( p+4, &traverse_data, FIRST );
			if ( data->success ) {
				data->instance = data->result;
				return EEnovInstanceType; } } }
	return EEnovNone;
}

//---------------------------------------------------------------------------
//	eenov_traversal
//---------------------------------------------------------------------------
BMTraverseCBSwitch( eenov_traversal )
case_( identifier_CB )
	CNInstance *x = data->instance;
	int success = 0;
	if ( !x->sub[0] ) {
		char *identifier = DBIdentifier( x, data->db );
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
		if (( x = CNSUB( x, is_f(FIRST)?0:1 ) )) {
			addItem( &data->stack.instance, data->instance );
			data->instance = x; }
		else {
			data->success = !!is_f( NEGATED );
			_prune( BM_PRUNE_TERM ) } }
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
	if ( *p=='?' ) {
		data->result = data->instance;
		if (!strncmp(p+1,":...",4))
			_return( 2 ) }
	data->success = !is_f( NEGATED );
	_break
case_( end_CB )
	_return( 1 )
BMTraverseCBEnd

//===========================================================================
//	eenov_query_op
//===========================================================================
static inline int eenov_op( EEnovQueryOp, CNInstance *, CNDB *, EEnovData * );

static CNInstance *
eenov_query_op( EEnovQueryOp op, BMContext *ctx, char *p, EEnovData *data )
/*
	Traverses %<(_!_)> using data->instance==%<!> as pivot
		invoking EEnovTraverseCB on every match
        returns current match on the callback's BM_DONE, and NULL otherwise
	Assumption: p points to either %<?:_> or %<!:_> or %<(_)>
*/
{
	CNDB *db = data->db;
	CNInstance *e = data->instance;
	int privy = db_deprecated( e, db );
	listItem *i = newItem( e ), *j;
	listItem *xpn = NULL;

	bm_locate_emark( p, &xpn );

	BMTraverseData traverse_data;
	traverse_data.user_data = data;
	traverse_data.stack = &data->stack.flags;

	data->stack.flags = NULL;
	data->stack.instance = NULL;
	CNInstance *success = NULL;
	listItem *trail = NULL;
	listItem *stack[ 2 ] = { NULL, NULL };
	enum { XPN_id, LIST_id };
	for ( ; ; ) {
PUSH_xpn:	PUSH( stack[ XPN_id ], xpn, POP_xpn )
		data->result = e;
		data->instance = e;
		traverse_data.done = 0;
		eenov_traversal( p, &traverse_data, FIRST );
		if (( data->success )) {
			if ( traverse_data.done==2 ) {
				/* we have %<(_!_,?:...)>
		               	             ^    ^----- current traverse_data.p
					      ---------- current data->stack.instance
				*/
				// Here we do NOT ward off doublons
				if ( eenov_op( op, data->result, db, data )==BM_DONE ) {
					success = data->result;
					goto RETURN; }
				e = popListItem( &data->stack.instance );
				popListItem( &data->stack.flags ); // flush
				for ( ; ; ) {
PUSH_list:				LUSH( stack[ LIST_id ], POP_list )
					if ( eenov_op( op, e, db, data )==BM_DONE ) {
						success = data->result;
						goto RETURN; }
					i = popListItem( &stack[ LIST_id ] );
					e = i->ptr; goto PUSH_list;
POP_list:				LOP( stack[ LIST_id ], PUSH_list, NULL ) }
				if (( stack[ XPN_id ] ))
					POP_XPi( stack[ XPN_id ], xpn )
				else goto RETURN; }
			else {
				if ( !lookupIfThere( trail, e ) ) { // ward off doublons
					addIfNotThere( &trail, e );
					if ( eenov_op( op, data->result, db, data )==BM_DONE ) {
						success = data->result;
						goto RETURN; } } } }
POP_xpn:	POP( stack[ XPN_id ], xpn, PUSH_xpn, NULL ) }
RETURN:
	POP_ALL( stack[ XPN_id ], xpn );
	freeListItem( &stack[ LIST_id ] );
	freeListItem( &trail );
	freeItem( i );
	return success;
}
static inline int
eenov_op( EEnovQueryOp op, CNInstance *e, CNDB *db, EEnovData *data )
{
	switch ( op ) {
	case EEnovOutputOp:
		bm_out_put( data->param.output.od, e, db );
		return BM_CONTINUE;
	case EEnovInformOp:
		e = bm_inform( 0, data->param.inform.ctx, e, db );
		if (( e )) addItem( &data->param.inform.result, e );
		return BM_CONTINUE;
	case EEnovLookupOp:
		if ( !data->param.lookup.db || ( e = bm_intake(
			data->param.lookup.ctx, data->param.lookup.db,
			e, db )) ) { data->result = e; return BM_DONE; }
		break;
	case EEnovMatchOp:
		if ( db_match( data->param.match.x, data->param.match.db,
			e, db ) ) { data->result = e; return BM_DONE; } }
	return BM_CONTINUE;
}

