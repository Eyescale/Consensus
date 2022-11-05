#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "cell.h"
#include "eeno.h"

//===========================================================================
//	eeno_output / eeno_inform / eeno_lookup / eeno_match
//===========================================================================
typedef struct {
	CNInstance *src;
	CNInstance *result;
} EEnoData;
static inline int eeno_read( BMContext *, char *, EEnoData * );

int
eeno_output( BMContext *ctx, int type, char *p )
{
	EEnoData data;
	switch ( eeno_read(ctx,p,&data) ) {
	case 0: break;
	case 1:
		db_outputf( stdout, BMContextDB( ctx ),
			( type=='s' ? "%s" : "%_" ), data.src );
		break;
	default: ;
		CNEntity *cell = BMProxyThat( data.src );
		db_outputf( stdout, BMContextDB( BMCellContext(cell) ),
			( type=='s' ? "%s" : "%_" ), data.result ); }
	return 0;
}
CNInstance *
eeno_inform( BMContext *ctx, CNDB *db, char *p, BMContext *dst )
{
	EEnoData data;
	switch ( eeno_read(ctx,p,&data) ) {
	case 0: return NULL;
	case 1: return ((dst) ? bm_inform_context(db,data.src,dst) : data.src );
	default: ;
		CNEntity *cell = BMProxyThat( data.src );
		CNDB *db_x = BMContextDB( BMCellContext(cell) );
		return bm_inform_context( db_x, data.result, ((dst)?dst:ctx) ); }
}
CNInstance *
eeno_lookup( BMContext *ctx, CNDB *db, char *p )
{
	EEnoData data;
	switch ( eeno_read(ctx,p,&data) ) {
	case 0: return NULL;
	case 1: return data.src;
	default: ;
		CNEntity *cell = BMProxyThat( data.src );
		CNDB *db_x = BMContextDB( BMCellContext(cell) );
		return bm_lookup_x( db_x, data.result, ctx, db ); }
}
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
		CNEntity *cell = BMProxyThat( data.src );
		CNDB *db_y = BMContextDB( BMCellContext(cell) );
		return db_x_match( db_x, x, db_y, data.result ); }
}

//---------------------------------------------------------------------------
//	eeno_read
//---------------------------------------------------------------------------
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
		for ( p++; *p!='>'; p++ ) {
			switch ( *p ) {
			case '(':
				add_item( &stack, position );
				addItem( &stack, sub );
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
				sub = popListItem( &stack );
				position = pop_item( &stack );
				break; } } }
	data->result = y;
	return 2;
}

//===========================================================================
//	db_x_match
//===========================================================================
int
db_x_match( CNDB *db_x, CNInstance *x, CNDB *db_y, CNInstance *y )
/*
	Assumption: x!=NULL
	Note that we use y to lead the traversal - but works either way
*/
{
	if ( !y ) return 0;
	if ( db_x==db_y ) return ( x==y );
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
				{ ndx=1; break; } } }
FAIL:
	freeListItem( &stack );
	return 0;
}

