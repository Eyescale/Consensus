#ifndef CONTEXT_H
#define CONTEXT_H

#include "database.h"
#include "narrative.h"

typedef struct {
	CNDB *db;
	Registry *registry;
}
BMContext;

BMContext * bm_push( CNNarrative *n, CNInstance *e, CNDB *db );
void bm_pop( BMContext * );
CNInstance * bm_lookup( int privy, char *p, BMContext *ctx );
CNInstance * bm_register( BMContext *, char *p, CNInstance *e );


#endif	// CONTEXT_H
