#ifndef INSTANTIATE_PRIVATE_H
#define INSTANTIATE_PRIVATE_H

#include "context.h"
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
	struct { void *level; char *p; } *bgn;
	struct { BMMark *mark; listItem *results; } *end;
	} LoopData;

#define NDX ( is_f( FIRST ) ? 0 : 1 )

#define ifn_instantiate_traversal( expr, extra ) \
	traversal->done = INFORMED|LITERAL|extra; \
	p = instantiate_traversal( expr, traversal, FIRST ); \
	if ( traversal->done==BMTraverseError || !data->sub[ 0 ] )
#define BM_ASSIGN( sub, db ) \
	for ( listItem *i=sub[0], *j=sub[1]; (i)&&(j); i=i->next, j=j->next ) \
		db_assign( i->ptr, j->ptr, db ); \
	freeListItem( &sub[ 0 ] ); \
	freeListItem( &sub[ 1 ] );
#define BM_UNASSIGN( sub, db ) \
	for (CNInstance*e;(e=popListItem( &sub[0] ));) \
		db_unassign( e, db );

#define CONNECTED( e )	((e->as_sub[0])||(e->as_sub[1]))

static inline int newborn_authorized( InstantiateData *data, int mask )
/*	current
		1st bit is set iff marked list has newborn
		2nd bit is set iff marked list.sub[0] has newborn
		3rd bit is set iff marked list.sub[1] has newborn
	if !mask then we want (current&6)==6 ie.
		both marked list.subs have newborn
	otherwise we simply test current&mask
*/ {
	int current = data->newborn.current;
	if ( current ) return mask ? (current&mask) : (current&6)==6;
	for ( listItem *i=data->newborn.stack; i!=NULL; i=i->next )
		if (( current=cast_i( i->ptr ) ))
			return mask ? (current&mask) : (current&6)==6;
	return 0; }


#endif	// INSTANTIATE_PRIVATE_H
