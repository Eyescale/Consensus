#ifndef CONTEXT_H
#define CONTEXT_H

#include "database.h"
#include "narrative.h"

typedef struct {
	CNDB *db;
	Registry *registry;
}
BMContext;
BMContext *  bm_push( CNNarrative *, CNInstance *, CNDB * );
void	     bm_pop( BMContext * );
char *       bm_locate_mark( char *, listItem ** );
listItem *   bm_push_mark( BMContext *, CNInstance * );	
CNInstance * bm_pop_mark( BMContext * );
CNInstance * lookup_mark_register( BMContext * );
CNInstance * bm_register( BMContext *, char * );
CNInstance * bm_lookup( int privy, char *, BMContext * );

#define AS_SUB  0
#define SUB     2
#ifdef UNIFIED
#define ESUB(e,ndx) ( e->sub[0] ? e->sub[ndx] : NULL )
#else
#define ESUB(e,ndx) e->sub[ndx]
#endif
void xpn_add( listItem **xp, int as_sub, int position );
void xpn_out( FILE *stream, listItem *xp );


#endif	// CONTEXT_H
