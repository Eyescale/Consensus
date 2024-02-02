#ifndef INSTANTIATE_PRIVATE_H
#define INSTANTIATE_PRIVATE_H

#include "scour.h"

typedef struct {
	struct { listItem *stack; int current; } newborn;
	struct { listItem *flags; } stack;
	listItem *sub[ 2 ];
	listItem *results;
	listItem *loop;
	BMContext *ctx, *carry;
	CNDB *db;
	} InstantiateData;
typedef struct {
	struct { void *level; char *p; } *info;
	MarkData *mark;
	} LoopData;

#define NDX ( is_f( FIRST ) ? 0 : 1 )

#define ifn_instantiate_traversal( expr, extra ) \
	traverse_data->done = INFORMED|LITERAL; \
	p = instantiate_traversal( expr, traverse_data, FIRST|extra ); \
	if ( traverse_data->done==BMTraverseError || !data->sub[ 0 ] )
#define BM_ASSIGN( sub, db ) \
	for ( listItem *i=sub[0], *j=sub[1]; (i)&&(j); i=i->next, j=j->next ) \
		db_assign( i->ptr, j->ptr, db ); \
	freeListItem( &sub[ 0 ] ); \
	freeListItem( &sub[ 1 ] );
#define BM_UNASSIGN( sub, db ) \
	for (CNInstance*e;(e=popListItem( &sub[0] ));) \
		db_unassign( e, db );

#define CONNECTED( e )	((e->as_sub[0])||(e->as_sub[1]))

static inline int newborn_authorized( InstantiateData *data ) {
	int current = data->newborn.current;
	if ( current ) return ( current > 0 );
	for ( listItem *i=data->newborn.stack; i!=NULL; i=i->next )
		if (( current=cast_i( i->ptr ) ))
			return ( current > 0 );
	return 0; }
static inline MarkData *markscan( char *p, BMContext *ctx ) {
	listItem *results = bm_scan( p, ctx );
	if (( results )) {
		Pair *mark = bm_mark( p, NULL );
		if ( !mark ) mark = newPair( cast_ptr(QMARK), NULL );
		mark->name = cast_ptr( AS_PER|cast_i(mark->name) );
		return (MarkData *) newPair( mark, results ); }
	return NULL; }


#endif	// INSTANTIATE_PRIVATE_H
