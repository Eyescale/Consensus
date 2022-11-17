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

#define BMContextId( ctx )	((Pair *) registryLookup( ctx, "" )->value )
#define BMContextSelf( ctx )	((CNInstance *) BMContextId(ctx)->name )
#define BMContextParent( ctx )	((CNInstance *) BMContextId(ctx)->value )
#define BMContextDB( ctx )	((CNDB *) registryLookup( ctx, "%" )->value )
#define BMContextActive( ctx )	((ActiveRV *) registryLookup( ctx, "@" )->value )

#define BMContextCell( ctx )	(DBProxyThat(BMContextSelf(ctx)))

inline CNInstance * BMContextPerso( BMContext *ctx ) {
	CNInstance *perso = registryLookup( ctx, "." )->value;
	return ( !perso ? BMContextSelf(ctx) : perso ); }


#endif	// CONTEXT_H
