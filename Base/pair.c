#include <stdio.h>
#include <stdlib.h>

#include "pair.h"
#include "cache.h"

#ifdef UCACHE
#define PairCache UCache
#else
static void **PairCache = NULL;
#endif

Pair *
newPair( void *name, void *value )
{
        Pair *pair = (Pair *) allocate( &PairCache );
        pair->name = name;
        pair->value = value;
        return pair;
}

void
freePair( Pair *pair )
{
	recycle((void**) pair, &PairCache );
}

