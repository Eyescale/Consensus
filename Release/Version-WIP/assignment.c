#include <stdio.h>
#include <stdlib.h>

#include "instantiate.h"
#include "assignment.h"
#include "string_util.h"
#include "locate.h"

//===========================================================================
//	bm_instantiate_assignment
//===========================================================================
static listItem * bm_assign( listItem *sub[2], BMContext *ctx );

void
bm_instantiate_assignment( char *expression, BMTraverseData *traverse_data )
{
	BMInstantiateData *data = traverse_data->user_data;
	char *p = expression + 1;
	listItem *sub[ 2 ];

	DBG_VOID( p )
	traverse_data->done = INFORMED;
	p = bm_traverse( p, traverse_data, FIRST );
	if ( traverse_data->done==2 || !data->sub[ 0 ] )
		return;
	sub[ 0 ] = data->sub[ 0 ];
	data->sub[ 0 ] = NULL;

	p++; // move past ','
	if ( !strncmp( p, "~.", 2 ) ) {
		sub[ 1 ] = NULL;
		data->sub[ 0 ] = bm_assign( sub, data->ctx );
		freeListItem( &sub[ 0 ] ); }
	else {
		DBG_VOID( p )
		traverse_data->done = INFORMED;
		p = bm_traverse( p, traverse_data, FIRST );
		if ( traverse_data->done==2 )
			freeListItem( &sub[ 0 ] );
		else {
			sub[ 1 ] = data->sub[ 0 ];
			data->sub[ 0 ] = bm_assign( sub, data->ctx );
			freeListItem( &sub[ 0 ] );
			freeListItem( &sub[ 1 ] ); } }
}

static listItem *
bm_assign( listItem *sub[2], BMContext *ctx )
/*
	Note that the number of assignments actually performed is
		INF( cardinal(sub[0]), cardinal(sub[1]) )
	And that nothing is done with the excess, if there is -
		except for sub[1]==NULL
*/
{
	CNDB *db = BMContextDB( ctx );
	listItem *results = NULL;
	if ( !sub[1] )
		for ( listItem *i=sub[0]; i!=NULL; i=i->next )
			addItem( &results, db_unassign( i->ptr, db ) );
	else {
		CNInstance *star = db_star( db );
		for ( listItem *i=sub[0], *j=sub[1]; (i) && (j); i=i->next, j=j->next ) {
			CNInstance *e = db_instantiate( star, i->ptr, db );
			addItem( &results, db_instantiate( e, j->ptr, db ) );
		} }
	return results;
}

//===========================================================================
//	bm_query_assignment
//===========================================================================
static inline CNInstance * assignment( CNInstance *e, CNDB *db );
static XPTraverseCB
	bm_verify_unassigned, bm_verify_variable, bm_verify_value;

CNInstance *
bm_query_assignment( BMQueryType type, char *expression, BMQueryData *data )
{
	BMContext *ctx = data->ctx;
	CNDB *db = BMContextDB( ctx );
	CNInstance *star = data->star;
	CNInstance *success = NULL, *e;
	expression++;
	char *value = p_prune( PRUNE_FILTER, expression ) + 1;
	if ( !strncmp( value, "~.", 2 ) ) {
		switch ( type ) {
		case BM_CONDITION: ;
			listItem *exponent = NULL;
			char *p = bm_locate_pivot( expression, &exponent );
			if (( p )) {
				e = bm_lookup( 0, p, ctx );
				if ( !e ) {
					freeListItem( &exponent );
					break; }
				data->exponent = exponent;
				data->pivot = newPair( p, e );
				success = xp_traverse( expression, data, bm_verify_unassigned );
				if (( success )) success = data->instance; // ( *, success )
				freePair( data->pivot );
				freeListItem( &exponent ); }
			else {
				for ( listItem *i=star->as_sub[0]; i!=NULL; i=i->next ) {
					CNInstance *e = i->ptr;
					if (( assignment(e,db) )) continue;
					if ( xp_verify( e->sub[1], expression, data ) ) {
						success = e; // return (*,.)
						break; } } }
			break;
		case BM_INSTANTIATED: ;
			listItem *s = NULL;
			for ( e=db_log(1,0,db,&s); e!=NULL; e=db_log(0,0,db,&s) ) {
				if ( e->sub[0]!=star || (assignment(e,db)) )
					continue;
				if ( xp_verify( e->sub[1], expression, data ) ) {
					freeListItem( &s );
					success = e; // return (*,.)
					break; } }
			// no break
		default:
			break; }
	}
	else {
		switch ( type ) {
		case BM_CONDITION: ;
			listItem *exponent = NULL;
			char *p = bm_locate_pivot( expression, &exponent );
			if (( p )) {
				e = bm_lookup( 0, p, ctx );
				if ( !e ) {
					freeListItem( &exponent );
					break; }
				data->exponent = exponent;
				data->pivot = newPair( p, e );
				data->user_data = value;
				success = xp_traverse( expression, data, bm_verify_value );
				if (( success )) success = data->instance; // ((*,success),.)
				freePair( data->pivot );
				freeListItem( &exponent ); }
			else {
				p = bm_locate_pivot( value, &exponent );
				if (( p )) {
					e = bm_lookup( 0, p, ctx );
					if ( !e ) {
						freeListItem( &exponent );
						break; }
					data->exponent = exponent;
					data->pivot = newPair( p, e );
					data->user_data = expression;
					success = xp_traverse( value, data, bm_verify_variable );
					if (( success )) success = data->instance; // ((*,.),success)
					freePair( data->pivot );
					freeListItem( &exponent ); }
				else {
					for ( listItem *i=star->as_sub[0]; i!=NULL; i=i->next ) {
						CNInstance *e = i->ptr, *f;
						if ( db_private(0,e,db) || !(f=assignment(e,db)) )
							continue;
						if ( xp_verify( e->sub[1], expression, data ) &&
						     xp_verify( f->sub[1], value, data ) ) {
							success = f;	// return ((*,.),.)
							break; } } } }
			break;
		case BM_INSTANTIATED: ;
			listItem *s = NULL;
			for ( e=db_log(1,0,db,&s); e!=NULL; e=db_log(0,0,db,&s) ) {
				CNInstance *f = ESUB( e, 0 );
				if ( !f || f->sub[0]!=star ) continue;
				if ( xp_verify( f->sub[1], expression, data ) &&
				     xp_verify( e->sub[1], value, data ) ) {
					freeListItem( &s );
					success = e; // return ((*,.),.)
					break; } }
			// no break
		default:
			break; } }
#ifdef DEBUG
	fprintf( stderr, "BM_QUERY_ASSIGNMENT: success=%d\n", !!success );
#endif
	return success;
}

static int
bm_verify_unassigned( CNInstance *e, char *variable, BMQueryData *data )
{
	if ( !xp_verify( e, variable, data ) ) return BM_CONTINUE;
	CNDB *db = BMContextDB( data->ctx );
	CNInstance *star = data->star;
	for ( listItem *i=e->as_sub[1], *j; i!=NULL; i=i->next ) {
		e = i->ptr; // e:(.,e)
		if ( e->sub[0]!=star || assignment(e,db) )
			continue;
		data->instance = e; // return (*,e)
		return BM_DONE;
	}
	return BM_CONTINUE;
}

static int
bm_verify_value( CNInstance *e, char *variable, BMQueryData *data )
{
	if ( !xp_verify( e, variable, data ) ) return BM_CONTINUE;
	CNDB *db = BMContextDB( data->ctx );
	CNInstance *star = data->star;
	char *value = data->user_data;
	for ( listItem *i=e->as_sub[1]; i!=NULL; i=i->next ) {
		CNInstance *f = i->ptr, *g; // f:(.,e)
		if ( f->sub[0]!=star || !(g=assignment(f,db)) )
			continue;
		if ( xp_verify( g->sub[1], value, data ) ) {
			data->instance = g; // return ((*,e),.)
			return BM_DONE;
		} }
	return BM_CONTINUE;
}

static int
bm_verify_variable( CNInstance *e, char *value, BMQueryData *data )
{
	if ( !xp_verify( e, value, data ) ) return BM_CONTINUE;
	CNDB *db = BMContextDB( data->ctx );
	CNInstance *star = data->star;
	char *variable = data->user_data;
	for ( listItem *i=e->as_sub[1], *j; i!=NULL; i=i->next ) {
		e = i->ptr; // e:(.,e)
		if ( e->sub[0]->sub[0]!=star || db_private(0,e,db) )
			continue;
		CNInstance *f = e->sub[ 0 ]; // e:(f:(*,.),e)
		if ( xp_verify( f->sub[1], variable, data ) ) {
			data->instance = e; // return ((*,.),e)
			return BM_DONE;
		} }
	return BM_CONTINUE;
}

static inline CNInstance *
assignment( CNInstance *e, CNDB *db )
{
	for ( listItem *j=e->as_sub[0]; j!=NULL; j=j->next )
		if ( !db_private( 0, j->ptr, db ) )
			return j->ptr;
	return NULL;
}

