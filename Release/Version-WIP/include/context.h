#ifndef CONTEXT_H
#define CONTEXT_H

#include "database.h"

typedef Registry BMContext;

BMContext *	newContext( CNEntity *cell, CNEntity *parent );
void		freeContext( BMContext * );
void		bm_context_finish( BMContext *ctx, int subscribe );
void		bm_context_activate( BMContext *ctx, CNInstance *proxy );
void		bm_context_init( BMContext *ctx );
void		bm_context_update( BMContext *ctx );
void 		bm_context_set( BMContext *, char *, CNInstance * );
int		bm_context_declare( BMContext *, char * );
void		bm_context_check( BMContext *ctx );
void		bm_context_flush( BMContext * );
void		bm_context_pipe_flush( BMContext * );
int		bm_context_mark( BMContext *, char *, CNInstance *, int *marked );
int		bm_context_mark_x( BMContext *, char *, CNInstance *, int *marked );
void 		bm_context_unmark( BMContext *, int );
listItem *	bm_push_mark( BMContext *, int, void * );	
void *		bm_pop_mark( BMContext *, int );
void *		bm_context_lookup( BMContext *, char * );
CNInstance *	bm_lookup( int privy, char *, BMContext *, CNDB * );
CNInstance *	bm_register( BMContext *, char *, CNDB *);
listItem *	bm_inform( BMContext *, CNDB *, listItem **, BMContext * );
CNInstance *	bm_context_inform( BMContext *, CNDB *, CNInstance *, BMContext * );

typedef struct {
	struct { listItem *activated, *deactivated; } *buffer;
	listItem *value;
} ActiveRV;

#define BMContextId( ctx )	((Pair *) registryLookup( ctx, "" )->value )
#define BMContextSelf( ctx )	((CNInstance *) BMContextId(ctx)->name )
#define BMContextParent( ctx )	((CNInstance *) BMContextId(ctx)->value )
#define BMContextDB( ctx )	((CNDB *) registryLookup( ctx, "%" )->value )
#define BMContextPerso( ctx )	((CNInstance *) registryLookup( ctx, "." )->value )
#define BMContextActive( ctx )	((ActiveRV *) registryLookup( ctx, "@" )->value )

#define isProxy( e )		((e->sub[0]) && !e->sub[1])
#define BMProxyThis( proxy )	((proxy)->sub[0]->sub[0])
#define BMProxyThat( proxy )	((proxy)->sub[0]->sub[1])
#define isProxySelf( proxy )	(BMProxyThis(proxy)==NULL)

#define BMContextThis( ctx )	(BMProxyThat(BMContextSelf(ctx)))

#endif	// CONTEXT_H
