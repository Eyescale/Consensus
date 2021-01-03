#include <stdio.h>
#include <stdlib.h>

#include "pair.h"

#define RECYCLE
#define C 500
static Pair *freePairList = NULL;

Pair *
newPair( void *name, void *value )
{
        Pair *pair;
        if ( freePairList == NULL ) {
#ifdef RECYCLE
		void **this = (void **) malloc(C*2*sizeof(void*));
		this[ 1 ] = NULL;
		for ( int i=C-1; (i--); ) {
			this += 2;
			this[ 1 ] = this-2;
		}
		pair = (Pair *) this;
		freePairList = (Pair *) this[ 1 ];
#else
		pair = malloc( sizeof( Pair ) );
#endif
	}
        else {
                pair = freePairList;
                freePairList = pair->value;
        }
        pair->name = name;
        pair->value = value;
        return pair;
}

void
freePair( Pair *pair )
{
#ifdef RECYCLE
        pair->value = freePairList;
	freePairList = pair;
#else
	if ( pair == NULL ) {
		fprintf( stderr, "freePair: NULL\n" );
		return;
	}
	pair->name = NULL;
	pair->value = NULL;
	free( pair );
#endif
}

