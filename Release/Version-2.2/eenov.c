#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "locate_emark.h"
#include "cell.h"

#include "eenov.h"
#include "eenov_private.h"
#include "eenov_traversal.h"

//===========================================================================
//	eenov_output / eenov_inform / eenov_lookup / eenov_match
//===========================================================================
static EEnovType eenov_type( BMContext *, char *, EEnovData * );
static int eenov_query( EEnovQueryOp, BMContext *, char *, EEnovData * );
static CNInstance *lookup_EEVA( CNInstance *, char *, CNDB * );

int
eenov_output( char *p, BMContext *ctx, OutputData *od ) {
	EEnovData data;	// mem NOT set
	switch ( eenov_type(ctx,p,&data) ) {
	case EEnovNone:
	case EEvaType:
		return 0;
	case EEnovSrcType: ;
		CNDB *db = BMContextDB( ctx );
		char *fmt = (*od->fmt=='s'?"%s":"%_");
		return db_outputf( db, od->stream, fmt, data.src );
	case EEnovInstanceType:
		fmt = (*od->fmt=='s'?"%s":"%_");
		return db_outputf( data.db, od->stream, fmt, data.instance );
	case EEnovExprType:
		data.param.output.od = od;
		eenov_query( EEnovOutputOp, ctx, p+2, &data );
		return bm_out_last( od, data.db ); } }

listItem *
eenov_inform( BMContext *ctx, CNDB *db, char *p, BMContext *dst ) {
	EEnovData data;	// mem NOT set
	CNInstance *e;
	switch ( eenov_type(ctx,p,&data) ) {
	case EEnovNone:
	case EEvaType:
		return NULL;
	case EEnovSrcType:
		e = bm_translate( dst, data.src, db, 1 );
		return ((e) ? newItem(e) : NULL );
	case EEnovInstanceType:
		e = bm_translate( ((dst)?dst:ctx), data.instance, data.db, 1 );
		return ((e) ? newItem(e) : NULL );
	case EEnovExprType:
		data.param.inform.ctx = ((dst)?dst:ctx);
		data.param.inform.result = NULL;
		eenov_query( EEnovInformOp, ctx, p+2, &data );
		reorderListItem( &data.param.inform.result );
		return data.param.inform.result; } }

CNInstance *
eenov_lookup( BMContext *ctx, CNDB *db, char *p )
/*
	Note that when db==NULL we return data.instance as is
*/ {
	EEnovData data;	// mem NOT set
	switch ( eenov_type(ctx,p,&data) ) {
	case EEnovNone:
	case EEvaType:
		return NULL;
	case EEnovSrcType:
		return data.src;
	case EEnovInstanceType:
		if ( !db ) return data.instance;
		return bm_translate( ctx, data.instance, data.db, 0 );
	case EEnovExprType:
		data.param.lookup.ctx = ctx;
		data.param.lookup.db = db;
		return eenov_query( EEnovLookupOp, ctx, p+2, &data ) ?
			data.result : NULL; } }

int
eenov_match( BMContext *ctx, char *p, CNInstance *x, CNDB *db_x )
/*
	Note that when invoked via query.c:match() then
		BMContextDB(ctx)==data.db==db_x
*/ {
	EEnovData data;	// mem NOT set
	switch ( eenov_type(ctx,p,&data) ) {
	case EEnovNone:
		return 0;
	case EEvaType:
		return !!lookup_EEVA( x, p, db_x );
	case EEnovSrcType:
		return db_match( x, db_x, data.src, BMContextDB(ctx) ); 
	case EEnovInstanceType:
		return db_match( x, db_x, data.instance, data.db );
	case EEnovExprType:
		data.param.match.x = x;
		data.param.match.db = db_x;
		return eenov_query( EEnovMatchOp, ctx, p+2, &data ); } }

//---------------------------------------------------------------------------
//	eenov_type
//---------------------------------------------------------------------------
static EEnovType
eenov_type( BMContext *ctx, char *p, EEnovData *data ) {
	EEnoRV *eenov;
	if ( p[2]=='.' || !strncmp(p+2,"::",2) )
		return EEvaType;
	else if (!( eenov=BMContextEENOV(ctx) ))
		return EEnovNone;

	data->success = 1; // initialize
	CNInstance *src = eenov->src;
	data->src = src;

	switch ( p[2] ) {
	case '!': data->instance = eenov->event->name; break;
	case '?': data->instance = eenov->event->value; break;
	case '(': data->instance = eenov->event->name; break;
	default: return EEnovSrcType; }

	CNEntity *cell = CNProxyThat( src );
	data->db = BMContextDB( BMCellContext(cell) );
	if ( p[2]=='(' )
		return EEnovExprType;
	else if ( p[3]=='>' )
		return EEnovInstanceType;
	else {
		BMTraverseData traversal;
		traversal.user_data = data;
		traversal.stack = &data->stack.flags;

		data->stack.flags = NULL;
		data->stack.instance = NULL;
		data->result = data->instance;
		traversal.done = 0;
		p = eenov_traversal( p+4, &traversal, FIRST );
		if ( !data->success ) return EEnovNone;

		data->instance = data->result;
		return EEnovInstanceType; } }

//===========================================================================
//	eenov_query
//===========================================================================
static inline int eenov_op( EEnovQueryOp, CNInstance *, CNDB *, EEnovData * );

static int
eenov_query( EEnovQueryOp op, BMContext *ctx, char *p, EEnovData *data )
/*
	Traverses %<(_!_)> using data->instance==%<!> as pivot
		invoking EEnovTraverseCB on every match
        returns current match on the callback's BMQ_DONE, and NULL otherwise
	Assumption: p points to either %<?:_> or %<!:_> or %<(_)>
*/ {
	CNDB *db = data->db;
	CNInstance *e = data->instance;
	int privy = db_deprecated( e, db );
	listItem *j, *i = newItem( e );
	listItem *xpn = NULL;

	bm_locate_emark( p, &xpn );
	for ( listItem *i=xpn; i!=NULL; i=i->next )
		i->ptr = cast_ptr( cast_i(i->ptr) & 1 );

	BMTraverseData traversal;
	traversal.user_data = data;
	traversal.stack = &data->stack.flags;

	data->stack.flags = NULL;
	data->stack.instance = NULL;
	struct { listItem *xpn, *list; } stack = { NULL, NULL };
	listItem *trail = NULL;
	int success = 0;
	for ( ; ; ) {
PUSH_xpn:	PUSH( stack.xpn, xpn, POP_xpn )
		if (( !addIfNotThere( &trail, e ) )) // ward off doublons Here
			goto POP_xpn;
		data->result = e;
		data->instance = e;
		traversal.done = 0;
		eenov_traversal( p, &traversal, FIRST );
		if ( data->success ) {
			e = data->result;
			if ( eenov_op( op, e, db, data )==BMQ_DONE ) {
				success = 1;
				goto RETURN; }
			if ( traversal.done==2 ) {
				/* we have %<(_!_,?:...)>
			              	     ^    ^----- current traversal.p
					      ---------- current data->stack.instance
				*/
				e = popListItem( &data->stack.instance );
				popListItem( &data->stack.flags ); // flush
PUSH_list:			LUSH( stack.list, POP_list )
				// Here we do NOT ward off doublons
				if ( eenov_op( op, e, db, data )==BMQ_DONE ) {
					success = 1;
					goto RETURN; }
				i = popListItem( &stack.list );
				e = i->ptr; goto PUSH_list;
POP_list:			LOP( stack.list, PUSH_list )
				if ( !stack.xpn )
					goto RETURN;
				POP_XPi( stack.xpn, xpn ) } }
POP_xpn:	POP( stack.xpn, xpn, PUSH_xpn ) }
RETURN:
	POP_ALL( stack.xpn, xpn );
	freeListItem( &stack.list );
	freeListItem( &trail );
	freeListItem( &xpn );
	freeItem( i );
	return success; }

static inline int
eenov_op( EEnovQueryOp op, CNInstance *e, CNDB *db, EEnovData *data ) {
	switch ( op ) {
	case EEnovOutputOp:
		bm_out_put( data->param.output.od, e, db );
		return BMQ_CONTINUE;
	case EEnovInformOp:
		e = bm_translate( data->param.inform.ctx, e, db, 1 );
		if (( e )) addItem( &data->param.inform.result, e );
		return BMQ_CONTINUE;
	case EEnovLookupOp:
		if ( !data->param.lookup.db ||
		   ( e=bm_translate( data->param.lookup.ctx, e, db, 0 )) ) {
			data->result = e; return BMQ_DONE; }
		break;
	case EEnovMatchOp:
		if ( db_match( data->param.match.x, data->param.match.db,
			e, db ) ) { data->result = e; return BMQ_DONE; } }
	return BMQ_CONTINUE; }

//---------------------------------------------------------------------------
//	eenov_traversal
//---------------------------------------------------------------------------
#define NDX 	( is_f(FIRST) ? 0 : 1 )

BMTraverseCBSwitch( eenov_traversal )
case_( identifier_CB )
	int success = db_match_identifier( data->instance, p );
	data->success = is_f( NEGATED ) ? !success : success;
	_break
case_( open_CB )
	if is_f_next( COUPLE ) {
		CNInstance *x = data->instance;
		if (( x = CNSUB(x,NDX) )) {
			addItem( &data->stack.instance, data->instance );
			data->instance = x; }
		else {
			data->success = !!is_f( NEGATED );
			_prune( BMCB_TERM, p ) } }
	_break
case_( comma_CB )
	if ( !data->success )
		_prune( BMCB_TERM, p+1 )
	else {
		listItem *stack = data->stack.instance;
		data->instance = ((CNInstance*)stack->ptr)->sub[ 1 ];
		_break }
case_( close_CB )
	if is_f( COUPLE )
		data->instance = popListItem( &data->stack.instance );
	if is_f_next( NEGATED ) data->success = !data->success;
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
//	lookup_EEVA
//===========================================================================
static CNInstance *
lookup_EEVA( CNInstance *instance, char *p, CNDB *db )
/*
	General case: %<::>
	we want to find ((*,x),y) in db_dst's manifested log
	where either
		db_dst: BMProxyDB( instance->sub[1] )
		x: db_dst( instance->sub[0]->sub[0] )
		y: db_dst( instance->sub[0]->sub[1] )
	  or, if the above fails
		db_dst: db
		x: instance->sub[0]
		y: instance->sub[1]
	Special case: %<.> and db_dst!=db
		if all of the above fails, we check (instance,.)
		presuming instance contributed to a guard Status
*/ {
	CNDB *db_dst;
	CNInstance *star;
	CNInstance *e = instance;
	CNInstance *f = CNSUB(e,0);
	CNInstance *g = CNSUB(e,1);
	if ( !f || !g ) return NULL;
	CNInstance *x = CNSUB(f,0);
	CNInstance *y = CNSUB(f,1);
	if (( x )&&( y )&&( cnIsProxy(g) )) {
		CNCell *that = CNProxyThat( g );
		BMContext *dst = BMCellContext( that );
		db_dst = BMContextDB( dst );
		if ( !db_dst ) return NULL;
		x = bm_translate( dst, x, db, 0 );
		y = bm_translate( dst, y, db, 0 );
		if ( !x || !y ) return NULL; }
	else { db_dst=db; x=f; y=g; }
	star = db_lookup( 0, "*", db_dst );
	if ( !star ) return NULL;
	listItem *s = NULL;
	for ( e=DBLog(1,0,db_dst,&s); e!=NULL; e=DBLog(0,0,db_dst,&s) ) {
		if ( e->sub[ 1 ]!=y ) continue;
		CNInstance *f = CNSUB(e,0);
		if (( f ) && f->sub[0]==star && f->sub[1]==x ) {
			freeListItem( &s );
			return e; } } // return ((*,x),y)
	if ( p[2]=='.' && db_dst!=db ) {
		// EEva failed as value assignment - check (instance,.)
		/* Here we consider instance as representing an external
		   condition contributing to a local guard, which it just
		   enabled => ( instance, ( %, !!:guard )) deprecated
		   However we perform only minimum verification */
		for ( listItem *i=instance->as_sub[0]; i!=NULL; i=i->next )
			if ( db_deprecated(i->ptr,db) ) return instance; }
	return NULL; }

