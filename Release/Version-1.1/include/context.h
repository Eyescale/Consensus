#ifndef CONTEXT_H
#define CONTEXT_H

#include "database.h"
#include "narrative.h"

typedef struct {
	CNDB *db;
	Registry *registry;
}
BMContext;
BMContext *  newContext( CNNarrative *, CNInstance *, CNDB * );
void	     freeContext( BMContext * );
char *       bm_locate_mark( char *, listItem ** );
listItem *   bm_push_mark( BMContext *, int, void * );	
void *	     bm_pop_mark( BMContext *, int );
void *	     lookup_mark_register( BMContext *, int );
CNInstance * bm_register( BMContext *, char * );
CNInstance * bm_lookup( int privy, char *, BMContext * );

#define AS_SUB  0
#define SUB     2
#ifdef UNIFIED
#define ESUB(e,ndx) ( e->sub[0] ? e->sub[ndx] : NULL )
#else
#define ESUB(e,ndx) e->sub[ndx]
#endif


#endif	// CONTEXT_H
