#ifndef CONTEXT_H
#define CONTEXT_H

#include "database.h"
#include "traverse.h"

typedef struct {
	CNInstance *this;
	Registry *registry;
} BMContext;

BMContext *	newContext( CNEntity *parent );
void		freeContext( BMContext * );
void		bm_context_finish( BMContext *ctx, int subscribe );
BMCBTake	bm_activate( CNInstance *, BMContext *, void * );
BMCBTake	bm_deactivate( CNInstance *, BMContext *, void * );
void		bm_context_check( BMContext *ctx );
void		bm_context_init( BMContext *ctx );
void		bm_context_update( BMContext *ctx );
void 		bm_context_set( BMContext *, char *, CNInstance * );
void		bm_context_flush( BMContext * );
void		bm_flush_pipe( BMContext * );
int		bm_context_mark( BMContext *, char *, CNInstance *, int *marked );
int		bm_context_mark_x( BMContext *, char *, CNInstance *, int *marked );
void 		bm_context_unmark( BMContext *, int );
listItem *	bm_push_mark( BMContext *, int, void * );	
void *		bm_pop_mark( BMContext *, int );
int		bm_context_register( BMContext *, char * );
void *		bm_context_lookup( BMContext *, char * );
CNInstance *	bm_lookup( int privy, char *, BMContext * );
CNInstance *	bm_register( BMContext *, char * );
listItem *	bm_inform( BMContext *, listItem **, CNEntity * );
CNInstance *	bm_context_inform( BMContext *, CNInstance *, CNEntity * );

typedef struct {
	struct { listItem *activated, *deactivated; } *buffer;
	listItem *value;
} ActiveRV;

#define BMThisDB( this )	((CNDB *) ((CNEntity *) this )->sub[ 0 ] )

#define BMContextDB( ctx )	BMThisDB( ctx->this )
#define BMContextCarry( ctx )	((listItem **) &ctx->this->sub[1])
#define BMContextActive( ctx )	((ActiveRV *) registryLookup( ctx->registry, "@" )->value )
#define BMContextId( ctx )	((Pair *) registryLookup( ctx->registry, "%" )->value )
#define BMContextSelf( ctx )	( BMContextId(ctx)->name )
#define BMContextParent( ctx )	( BMContextId(ctx)->value )
#define BMContextPerso( ctx )	((CNInstance *) registryLookup( ctx->registry, "." )->value )

#define isProxy( e )		((e->sub[0]) && !e->sub[1])
#define BMProxyThat( proxy )	((proxy)->sub[0]->sub[1])
#define BMContextMatchSelf( ctx, e ) ( isProxy(e) && BMProxyThat(e)==ctx->this )


#endif	// CONTEXT_H
