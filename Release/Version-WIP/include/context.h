#ifndef CONTEXT_H
#define CONTEXT_H

#include "database.h"

typedef struct {
	CNInstance *this;
	Registry *registry;
} BMContext;

#define BMContextDB(ctx) ((CNDB *) ctx->this->sub[1])

BMContext *	newContext( CNDB * );
void		freeContext( BMContext * );
void 		bm_context_set( BMContext *, char *, CNInstance * );
void		bm_context_clear( BMContext * );
int		bm_context_mark( BMContext *, char *, CNInstance *, int *marked );
int		bm_context_register( BMContext *, char * );
listItem *	bm_push_mark( BMContext *, int, void * );	
void *		bm_pop_mark( BMContext *, int );
void *		bm_context_lookup( BMContext *, char * );
CNInstance *	bm_lookup( int privy, char *, BMContext * );
CNInstance *	bm_register( BMContext *, char * );


#endif	// CONTEXT_H
