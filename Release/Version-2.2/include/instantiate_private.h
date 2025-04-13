#ifndef INSTANTIATE_PRIVATE_H
#define INSTANTIATE_PRIVATE_H

#include "context.h"
#include "scour.h"

typedef struct {
	struct { listItem *flags; } stack;
	listItem *sub[ 2 ];
	listItem *results;
	listItem *loop;
	BMContext *ctx, *carry;
	CNDB *db;
	} InstantiateData;

#define NDX \
	( is_f( FIRST ) ? 0 : 1 )
#define CONNECTED( e )	\
	((e->as_sub[0])||(e->as_sub[1]))

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

typedef struct {
	struct { void *level; char *p; } *bgn;
	struct { BMMark *mark; listItem *results; } *end;
	} LoopData;

static inline void
loopPop( InstantiateData *data, int once ) {
	listItem **stack = &data->loop;
	for ( ; ; ) {
		LoopData *loop = popListItem( stack );
		if ( !loop ) return;
		if ( !once ) freeListItem( &loop->end->results );
		freePair((Pair *) loop->bgn );
		freePair((Pair *) loop->end );
		freePair((Pair *) loop );
		if ( once ) return; } }

#ifdef DEBUG
static inline void
DBG_CLEANUP( InstantiateData *data, char *expression ) {
	if ( !data->sub[0] && ( expression ))
		errout( InstantiatePartial, expression );
	if ( !data->sub[1] && !data->stack.flags && !data->results && !BMContextRVV(data->ctx,"%|") )
		return;

	fprintf( stderr, ">>>>> B%%: Error: bm_instantiate: memory leak" );
	if (( expression )) {
		fprintf( stderr, "\t\tdo %s\n\t<<<<< failed", expression );
		if ( !strmatch(":!\"", *expression ) ) {
			fprintf( stderr, " in expression\n" ); }
		else	fprintf( stderr, " in assignment\n" ); }
	else fprintf( stderr, " in assignment\n" );
	fprintf( stderr, " on:" );
	if ((data->sub[1])) {
		fprintf( stderr, " data->sub[ 1 ]" );
		if (!data->sub[0]) fprintf( stderr, "/!data->sub[ 0 ]" ); }
	if ((data->stack.flags)) fprintf( stderr, " data->stack.flags" );
	if ((data->results)) fprintf( stderr, " data->results" );
	if ((BMContextRVV(data->ctx,"%|"))) fprintf( stderr, " %%|" );
	fprintf( stderr, "\n" );

	loopPop( data, 0 );
	freeListItem( &data->sub[ 1 ] );
	freeListItem( &data->stack.flags );
	listItem *instances;
	listItem **results = &data->results;
	while (( instances = popListItem(results) ))
		freeListItem( &instances );
	bm_context_pipe_flush( data->ctx );
	exit( -1 ); }
#else
#define DBG_CLEANUP( traversal, expression )
#endif

#endif	// INSTANTIATE_PRIVATE_H
