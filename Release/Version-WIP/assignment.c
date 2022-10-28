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
bm_instantiate_assignment( char *expression, BMTraverseData *traverse_data, CNStory *story )
{
	BMInstantiateData *data = traverse_data->user_data;
	char *p = expression + 1; // skip leading ':'
	listItem *sub[ 2 ];

	DBG_VOID( p )
	traverse_data->done = INFORMED;
	p = bm_traverse( p, traverse_data, FIRST );
	if ( traverse_data->done==2 || !data->sub[ 0 ] )
		return;
	sub[ 0 ] = data->sub[ 0 ];
	data->sub[ 0 ] = NULL;
	p++; // move past ',' - aka. ':'

	if ( *p=='!' ) {
		p += 2; // skip '!!'
		CNDB *db = BMContextDB( data->ctx );
		Pair *entry = registryLookup( story, p );
		if (( entry )) {
			char *q = p = p_prune( PRUNE_IDENTIFIER, p );
			for ( listItem *i=sub[0]; i!=NULL; i=i->next, p=q ) {
				CNInstance *proxy = bm_conceive( entry, p, traverse_data );
				db_assign( i->ptr, proxy, db ); } }
		else {
			fprintf( stderr, ">>>>> B%%: Error: class not found in expression\n"
				"\t\tdo !! %s\n\t<<<<<\n", p );
			exit( -1 ); } }
	else if ( !strncmp( p, "~.", 2 ) ) {
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
		case sub[1]==NULL excepted
*/
{
	CNDB *db = BMContextDB( ctx );
	listItem *results = NULL;
	if ( !sub[1] )
		for ( listItem *i=sub[0]; i!=NULL; i=i->next )
			addItem( &results, db_unassign( i->ptr, db ) );
	else
		for ( listItem *i=sub[0], *j=sub[1]; (i)&&(j); i=i->next, j=j->next )
			addItem( &results, db_assign( i->ptr, j->ptr, db ) );
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
	expression++; // skip leading ':'
	char *value = p_prune( PRUNE_FILTER, expression ) + 1;
	if ( !strncmp( value, "~.", 2 ) ) {
		listItem *exponent = NULL;
		char *p = bm_locate_pivot( expression, &exponent );
		if (( p )) {
			e = bm_lookup( 0, p, ctx );
			if ( !e ) {
				freeListItem( &exponent );
				return NULL; }
			data->exponent = exponent;
			data->pivot = newPair( p, e );
			success = xp_traverse( expression, data, bm_verify_unassigned );
			if (( success )) success = data->instance; // ( *, success )
			freePair( data->pivot );
			freeListItem( &exponent ); }
		else {
			switch ( type ) {
			case BM_CONDITION: ;
				for ( listItem *i=star->as_sub[0]; i!=NULL; i=i->next ) {
					CNInstance *e = i->ptr;
					if (( assignment(e,db) )) continue;
					if ( xp_verify( e->sub[1], expression, data ) ) {
						success = e; // return (*,.)
						break; } }
				break;
			default: ;
				listItem *s = NULL;
				for ( e=db_log(1,0,db,&s); e!=NULL; e=db_log(0,0,db,&s) ) {
					if ( e->sub[0]!=star || (assignment(e,db)) )
						continue;
					if ( xp_verify( e->sub[1], expression, data ) ) {
						freeListItem( &s );
						success = e; // return (*,.)
						break; } }
				break;
			} } }
	else {
		listItem *exponent = NULL;
		char *p = bm_locate_pivot( expression, &exponent );
		if (( p )) {
			e = bm_lookup( 0, p, ctx );
			if ( !e ) {
				freeListItem( &exponent );
				return NULL; }
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
					return NULL; }
				data->exponent = exponent;
				data->pivot = newPair( p, e );
				data->user_data = expression;
				success = xp_traverse( value, data, bm_verify_variable );
				if (( success )) success = data->instance; // ((*,.),success)
				freePair( data->pivot );
				freeListItem( &exponent ); }
			else {
				switch ( type ) {
				case BM_CONDITION: ;
					for ( listItem *i=star->as_sub[0]; i!=NULL; i=i->next ) {
						CNInstance *e = i->ptr, *f;
						if ( db_private(0,e,db) || !(f=assignment(e,db)) )
							continue;
						if ( xp_verify( e->sub[1], expression, data ) &&
						     xp_verify( f->sub[1], value, data ) ) {
							success = f;	// return ((*,.),.)
							break; } }
					break;
				default: ;
					listItem *s = NULL;
					for ( e=db_log(1,0,db,&s); e!=NULL; e=db_log(0,0,db,&s) ) {
						CNInstance *f = CNSUB( e, 0 );
						if ( !f || f->sub[0]!=star ) continue;
						if ( xp_verify( f->sub[1], expression, data ) &&
						     xp_verify( e->sub[1], value, data ) ) {
							freeListItem( &s );
							success = e; // return ((*,.),.)
							break; } }
					break;
			} } } }
#ifdef DEBUG
	fprintf( stderr, "BM_QUERY_ASSIGNMENT: success=%d\n", !!success );
#endif
	return success;
}
static int
bm_verify_unassigned( CNInstance *e, char *variable, BMQueryData *data )
{
	if ( !xp_verify( e, variable, data ) )
		return BM_CONTINUE;
	int manifest = ( data->type==BM_INSTANTIATED );
	CNDB *db = BMContextDB( data->ctx );
	CNInstance *star = data->star;
	for ( listItem *i=e->as_sub[1]; i!=NULL; i=i->next ) {
		e = i->ptr; // e:(.,e)
		if ( e->sub[0]!=star ) continue;
		if ( manifest && !db_manifested(e,db) )
			continue;
		if ( assignment(e,db) ) continue;
		data->instance = e; // return (*,e)
		return BM_DONE; }
	return BM_CONTINUE;
}
static int
bm_verify_value( CNInstance *e, char *variable, BMQueryData *data )
{
	if ( !xp_verify( e, variable, data ) )
		return BM_CONTINUE;
	int manifest = ( data->type==BM_INSTANTIATED );
	CNDB *db = BMContextDB( data->ctx );
	CNInstance *star = data->star;
	char *value = data->user_data;
	for ( listItem *i=e->as_sub[1]; i!=NULL; i=i->next ) {
		CNInstance *f = i->ptr, *g; // f:(.,e)
		if ( f->sub[0]!=star || !(g=assignment(f,db)) )
			continue;
		if ( manifest && !db_manifested(g,db) )
			continue;
		if ( xp_verify( g->sub[1], value, data ) ) {
			data->instance = g; // return ((*,e),.)
			return BM_DONE; } }
	return BM_CONTINUE;
}
static int
bm_verify_variable( CNInstance *e, char *value, BMQueryData *data )
{
	if ( !xp_verify( e, value, data ) )
		return BM_CONTINUE;
	int manifest = ( data->type==BM_INSTANTIATED );
	CNDB *db = BMContextDB( data->ctx );
	CNInstance *star = data->star;
	char *variable = data->user_data;
	for ( listItem *i=e->as_sub[1]; i!=NULL; i=i->next ) {
		e = i->ptr; // e:(.,e)
		if ( e->sub[0]->sub[0]!=star || db_private(0,e,db) )
			continue;
		if ( manifest && !db_manifested(e,db) )
			continue;
		CNInstance *f = e->sub[ 0 ]; // e:(f:(*,.),e)
		if ( xp_verify( f->sub[1], variable, data ) ) {
			data->instance = e; // return ((*,.),e)
			return BM_DONE; } }
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

//===========================================================================
//	bm_proxy_feel_assignment
//===========================================================================
CNInstance *
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
		while (( e = popListItem( &candidates ) )) {
			data->x = e->sub[ 0 ];
			bm_traverse( expression, traverse_data, FIRST );
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
			bm_traverse( expression, traverse_data, FIRST );
			if ( traverse_data->done==2 ) {
				freeListItem( &data->stack.flags );
				freeListItem( &data->stack.x );
				traverse_data->done = 0;
				continue; }
			data->x = e->sub[ 1 ];
			traverse_data->done = 0;
			bm_traverse( value, traverse_data, FIRST );
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

