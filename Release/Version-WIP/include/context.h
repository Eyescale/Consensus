#ifndef CONTEXT_H
#define CONTEXT_H

#include "database.h"
#include "traverse.h"

typedef struct {
	CNInstance *this;
	Registry *registry;
} BMContext;

#define BMContextDB( ctx ) ((CNDB *) ctx->this->sub[0])
#define BMContextCarry( ctx ) ((listItem **) &ctx->this->sub[1])
#define BMContextParent( ctx ) (((Pair *) registryLookup( ctx->registry, "%" )->value )->name )
#define BMContextSelf( ctx ) (((Pair *) registryLookup( ctx->registry, "%" )->value )->value )
#define BMContextMatchSelf( ctx, e ) ( !e->sub[1] && (e->sub[0]) && e->sub[0]->sub[1]==ctx->this )

BMContext *	newContext( CNDB *, CNEntity *parent );
void		freeContext( BMContext * );
void		bm_context_finish( BMContext *ctx, int subscribe );
BMCBTake	bm_activate( CNInstance *, BMContext *, void * );
BMCBTake	bm_deactivate( CNInstance *, BMContext *, void * );
void		bm_context_init( BMContext *ctx );
void		bm_context_update( BMContext *ctx );
void 		bm_context_set( BMContext *, char *, CNInstance * );
CNInstance *	bm_context_fetch( BMContext *, char * );
CNInstance *	bm_context_inform( BMContext *, CNInstance *, BMContext * );
void		bm_context_flush( BMContext * );
void		bm_flush_pipe( BMContext * );
int		bm_context_mark( BMContext *, char *, CNInstance *, int *marked );
void 		bm_context_unmark( BMContext *, int );
listItem *	bm_push_mark( BMContext *, int, void * );	
void *		bm_pop_mark( BMContext *, int );
int		bm_context_register( BMContext *, char * );
void *		bm_context_lookup( BMContext *, char * );
CNInstance *	bm_lookup( int privy, char *, BMContext * );
CNInstance *	bm_register( BMContext *, char * );


#endif	// CONTEXT_H
