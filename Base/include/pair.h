#ifndef PAIR_H
#define PAIR_H

#include "list.h"

typedef struct {
	void *name;
	void *value;
} Pair;

Pair *newPair( void *name, void *value );
void freePair( Pair *pair );

#endif	// PAIR_H
