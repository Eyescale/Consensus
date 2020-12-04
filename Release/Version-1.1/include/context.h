#ifndef CONTEXT_H
#define CONTEXT_H

#include "database.h"
#include "narrative.h"

char * bm_locate_mark( char *expression, listItem **exponent );

typedef struct {
	CNDB *db;
	Registry *registry;
}
BMContext;
BMContext *  bm_push( CNNarrative *n, CNInstance *e, CNDB *db );
void	     bm_pop( BMContext * );
listItem *   bm_push_mark( BMContext *, CNInstance * );	
CNInstance * bm_pop_mark( BMContext * );
CNInstance * bm_register( BMContext *, char *p, CNInstance *e );
CNInstance * bm_lookup( int privy, char *p, BMContext *ctx );

#define AS_SUB  0
#define SUB     2
void xpn_add( listItem **xp, int as_sub, int position );
void xpn_out( FILE *stream, listItem *xp );


#endif	// CONTEXT_H
