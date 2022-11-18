#ifndef CONTEXT_H
#define CONTEXT_H

#include "database.h"

typedef Registry BMContext;

BMContext *	newContext( CNEntity *cell, CNEntity *parent );
void		freeContext( BMContext * );
void		bm_context_finish( BMContext *ctx, int subscribe );
void		bm_context_activate( BMContext *ctx, CNInstance *proxy );
void		bm_context_init( BMContext *ctx );
int		bm_context_update( CNEntity *this, BMContext *ctx );
void 		bm_context_set( BMContext *, char *, CNInstance * );
int		bm_declare( BMContext *, char * );
void		bm_context_clear( BMContext * );
void		bm_context_pipe_flush( BMContext * );
int		bm_context_mark( BMContext *, char *, CNInstance *, int *marked );
int		bm_context_mark_x( BMContext *, char *, char *, CNInstance *, CNInstance *, int * );
void 		bm_context_unmark( BMContext *, int );
listItem *	bm_push_mark( BMContext *, char *, void * );	
void		bm_pop_mark( BMContext *, char * );
void *		bm_context_lookup( BMContext *, char * );
CNInstance *	bm_lookup( int privy, char *, BMContext *, CNDB * );
CNInstance *	bm_lookup_x( CNDB *, CNInstance *, BMContext *, CNDB * );
CNInstance *	bm_register( BMContext *, char *, CNDB *);
listItem *	bm_inform( CNDB *, listItem **, BMContext * );
CNInstance *	bm_inform_context( CNDB *, CNInstance *, BMContext * );

typedef struct {
	struct { listItem *activated, *deactivated; } *buffer;
	listItem *value;
} ActiveRV;

typedef struct {
	Pair *event;
	CNInstance *src;
} EEnoRV;

inline Pair * BMContextId( BMContext *ctx )
	{ return (Pair *) registryLookup( ctx, "" )->value; }
inline CNInstance * BMContextSelf( BMContext *ctx )
	{ return (CNInstance *) BMContextId( ctx )->name; }
inline CNInstance * BMContextParent( BMContext *ctx )
	{ return (CNInstance *) BMContextId( ctx )->value; }
inline CNDB * BMContextDB( BMContext *ctx )
	{ return (CNDB *) registryLookup( ctx, "%" )->value; }
inline ActiveRV * BMContextActive( BMContext *ctx )
	{ return (ActiveRV *) registryLookup( ctx, "@" )->value; }
inline CNInstance * BMContextPerso( BMContext *ctx ) {
	CNInstance *perso = registryLookup( ctx, "." )->value;
	return ( !perso ? BMContextSelf(ctx) : perso ); }

inline CNEntity * BMContextCell( BMContext *ctx )
	{ return DBProxyThat( BMContextSelf(ctx) ); }


#endif	// CONTEXT_H
