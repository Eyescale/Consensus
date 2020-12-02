#ifndef CONTEXT_H
#define CONTEXT_H

#include "database.h"
#include "narrative.h"

typedef struct {
	CNDB *db;
	Registry *registry;
}
BMContext;

BMContext *  bm_push( CNNarrative *n, CNInstance *e, CNDB *db );
void	     bm_pop( BMContext * );
listItem *   push_mark_register( BMContext *, CNInstance * );	
CNInstance * pop_mark_register( BMContext * );
CNInstance * bm_register( BMContext *, char *p, CNInstance *e );
CNInstance * bm_lookup( int privy, char *p, BMContext *ctx );


#endif	// CONTEXT_H
