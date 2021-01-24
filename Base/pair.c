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


Pair *
new_pair( int name, int value )
{
	union { int value; void *ptr; } icast[ 2 ];
	icast[ 0 ].value = name;
	icast[ 1 ].value = value;
	return newPair( icast[ 0 ].ptr, icast[ 1 ].ptr );
}

void
freePair( Pair *pair )
{
	recycle((void**) pair, &PairCache );
}

