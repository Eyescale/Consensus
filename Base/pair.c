#include <stdio.h>
#include <stdlib.h>

#include "cache.h"
#include "pair.h"

#define C 500
static void **PairCache = NULL;

Pair *
newPair( void *name, void *value )
{
        Pair *pair = (Pair *) allocate( &PairCache, C );
        pair->name = name;
        pair->value = value;
        return pair;
}

void
freePair( Pair *pair )
{
	recycle((void**) pair, &PairCache );
}

