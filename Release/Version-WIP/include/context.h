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
int		bm_context_mark( BMContext *, char *, CNInstance *, int *marked );
int		bm_context_mark_x( BMContext *, char *, char *, Pair *, int * );
void 		bm_context_unmark( BMContext *, int );
listItem *	bm_push_mark( BMContext *, char *, void * );	
void		bm_pop_mark( BMContext *, char * );
int		bm_context_register( BMContext *, char * );
void *		bm_context_lookup( BMContext *, char * );
CNInstance *	bm_lookup( int privy, char *, BMContext *, CNDB * );
CNInstance *	bm_lookup_x( BMContext *, CNDB *, CNInstance *, CNDB * );
CNInstance *	bm_register( BMContext *, char *, CNDB *);
listItem *	bm_inform( BMContext *, listItem **, CNDB * );
CNInstance *	bm_inform_context( BMContext *, CNInstance *, CNDB * );

typedef struct {
	struct { listItem *activated, *deactivated; } *buffer;
	listItem *value;
} ActiveRV;

typedef struct {
	Pair *event;
	CNInstance *src;
} EEnoRV;

static inline Pair * BMContextId( BMContext *ctx )
	{ return registryLookup( ctx, "" )->value; }
static inline CNInstance * BMContextSelf( BMContext *ctx )
	{ return BMContextId( ctx )->name; }
static inline CNInstance * BMContextParent( BMContext *ctx )
	{ return BMContextId( ctx )->value; }
static inline CNDB * BMContextDB( BMContext *ctx )
	{ return registryLookup( ctx, "%" )->value; }
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
