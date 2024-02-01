#ifndef ARENA_H
#define ARENA_H

#include "database.h"
#include "registry.h"

typedef Pair CNArena;

CNArena *	newArena( void );
void		freeArena( CNArena * );
CNInstance *	bm_arena_register( CNArena *, char *, CNDB * );
void		bm_arena_deregister( CNArena *, CNInstance *, CNDB * );
CNInstance *	bm_arena_lookup( CNArena *, char *, CNDB * );
CNInstance *	bm_arena_translate( CNInstance *, CNDB * );
CNInstance *	bm_arena_transpose( CNInstance *, CNDB * );
void		bm_arena_flush( CNArena *, Pair *, CNDB *, listItem ** );

static inline int bm_arena_cmp( CNInstance *x, CNInstance *y ) {
	return ( x->sub[1]!=y->sub[1] ); }


#endif // ARENA_H
