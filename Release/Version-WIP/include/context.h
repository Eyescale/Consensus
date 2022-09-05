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
int		bm_context_mark( BMContext *, char *, CNInstance *, int * );
listItem *	bm_push_mark( BMContext *, int, void * );	
void *		bm_pop_mark( BMContext *, int );
void *		lookup_mark_register( BMContext *, int );
CNInstance *	bm_register( BMContext *, char * );
CNInstance *	bm_lookup( int privy, char *, BMContext * );


#endif	// CONTEXT_H
