#include <stdio.h>
#include <stdlib.h>

#include "pair.h"

#define U_CACHE
// #define U_COUNT

//===========================================================================
//	allocation functions
//===========================================================================
#ifdef U_CACHE

#define U_CACHE_SIZE 15000
#ifdef U_COUNT
static int UCount = 0;
#endif

inline void *
allocate( void ***Cache )
{
	void **this, **cache = *Cache;
	if (( cache ))
        	this = cache;
	else {
#ifdef U_COUNT
		fprintf( stderr, "B%%: allocate: %d\n", UCount++ );
#endif
		this = malloc( U_CACHE_SIZE * 2 * sizeof(void*) );
		this[ 1 ] = NULL;
		for ( int i=U_CACHE_SIZE-1; (i--); ) {
			this += 2;
			this[ 1 ] = this-2; } }
 	*Cache = this[ 1 ];
	return this;
}
inline void
recycle( void **item, void ***Cache )
{
	item[ 1 ] = *Cache;
	*Cache = item;
}

#else	// U_CACHE

inline void *
allocate( void ***cache )
{
	return calloc(1,2*sizeof(void*));
//	return malloc(2*sizeof(void*));
}
inline void
recycle( void **item, void ***Cache )
{
	if (( item )) free((void *) item );
	else fprintf( stderr, "***** Warning: recycle(): NULL\n" );
}

#endif	// U_CACHE

//===========================================================================
//	newPair / freePair
//===========================================================================
static void **PairCache = NULL;

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
Pair *
new_pair( int name, int value )
{
	union { int value; void *ptr; } icast[ 2 ];
	icast[ 0 ].value = name;
	icast[ 1 ].value = value;
	return newPair( icast[ 0 ].ptr, icast[ 1 ].ptr );
}

