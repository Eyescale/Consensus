#ifndef CONTEXT_H
#define CONTEXT_H

#include "database.h"

typedef Registry BMContext;

BMContext *	newContext( CNEntity *cell, CNEntity *parent );
void		freeContext( BMContext * );
void		bm_context_init( BMContext *ctx );
int		bm_context_update( CNEntity *this, BMContext *ctx );
void 		bm_context_actualize( BMContext *, char *, CNInstance * );
void		bm_context_release( BMContext * );

Pair *		bm_mark( int, char *, char *, void * );
void		bm_context_mark( BMContext *, Pair * );
Pair * 		bm_context_unmark( BMContext *, Pair * );
listItem *	bm_push_mark( BMContext *, char *, void * );	
void		bm_pop_mark( BMContext *, char * );

CNInstance *	bm_register( BMContext *, char *, CNDB * );
CNInstance *	bm_lookup( BMContext *, char *, int, CNDB * );
int		bm_match( BMContext *, CNDB *, char *, CNInstance *, CNDB * );
int		bm_context_register( BMContext *, char * );
void *		bm_context_lookup( BMContext *, char * );
void *		bm_inform( int, BMContext *, void *, CNDB * );
CNInstance *	bm_intake( BMContext *, CNDB *, CNInstance *, CNDB * );

typedef struct {
	struct { listItem *activated, *deactivated; } *buffer;
	listItem *value;
} ActiveRV;

typedef struct {
	Pair *event;
	CNInstance *src;
} EEnoRV;

static inline CNDB * BMContextDB( BMContext *ctx )
	{ return registryLookup( ctx, "" )->value; }
static inline Pair * BMContextId( BMContext *ctx )
	{ return registryLookup( ctx, "%" )->value; }
static inline CNInstance * BMContextSelf( BMContext *ctx )
	{ return BMContextId( ctx )->name; }
static inline CNInstance * BMContextParent( BMContext *ctx )
	{ return BMContextId( ctx )->value; }
static inline ActiveRV * BMContextActive( BMContext *ctx )
	{ return registryLookup( ctx, "@" )->value; }
static inline CNInstance * BMContextPerso( BMContext *ctx )
	{ return registryLookup( ctx, "." )->value; }

static inline CNEntity * BMContextCell( BMContext *ctx )
	{ return DBProxyThat( BMContextSelf(ctx) ); }

//===========================================================================
//	Utilities
//===========================================================================
static inline void bm_context_pipe_flush( BMContext *ctx ) {
	Pair *entry = registryLookup( ctx, "|" );
	freeListItem((listItem **) &entry->value ); }


#endif	// CONTEXT_H
