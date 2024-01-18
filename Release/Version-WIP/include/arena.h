#ifndef ARENA_H
#define ARENA_H

#include "database.h"
#include "registry.h"

void		freeArena( Registry * );
CNInstance *	bm_arena_register( Registry *, char *, CNDB * );
void		bm_arena_deregister( Registry *, CNInstance *, CNDB * );

static inline Registry * newArena( void ) {
	return newRegistry(IndexedByCharacter); }

#endif // ARENA_H
