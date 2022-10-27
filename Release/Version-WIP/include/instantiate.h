#ifndef INSTANTIATE_H
#define	INSTANTIATE_H

#include "expression.h"
#include "traverse.h"
#include "program.h"

// #define DEBUG

#ifdef DEBUG
#define DBG_VOID( p ) \
	if ( bm_void( p, ctx ) ) { \
		fprintf( stderr, ">>>>> B%%: bm_instantiate(): VOID: %s\n", p ); \
		exit( -1 ); }
#else
#define	DBG_VOID( p )
#endif

typedef struct {
	struct { listItem *flags; } stack;
	listItem *sub[ 2 ];
	listItem *results;
	BMContext *ctx;
	CNEntity *carry;
} BMInstantiateData;

void bm_instantiate_assignment( char *, BMTraverseData *, CNStory * );
CNInstance *bm_conceive( Pair *, char *, BMTraverseData * );


#endif	// INSTANTIATE_H
