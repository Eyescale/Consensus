#include <stdio.h>
#include <stdlib.h>

#include "pair.h"

#define CN_CACHE
#define CN_CACHE_COUNT
#define CN_CACHE_BSIZE 15000

//===========================================================================
//	newPair, freePair
//===========================================================================
#ifdef CN_CACHE
static inline void * allocate( void *** );
static inline void recycle( void **item, void ***Cache );
static struct {
	void **ptr;
	uint64_t size;
	uint64_t used;
	} CNCache = { NULL, 0, 0 };

uint64_t cnCacheGetSize( void ) { return CNCache.size; }
uint64_t cnCacheGetUsed( void ) { return CNCache.used; }

#ifdef CN_CACHE_COUNT
Pair *
newPair( void *name, void *value ) {
        Pair *pair = (Pair *) allocate( &CNCache.ptr );
	CNCache.used++;
        pair->name = name;
        pair->value = value;
        return pair; }
void
freePair( Pair *pair ) {
	recycle((void**) pair, &CNCache.ptr );
	CNCache.used--; }

#else // !CN_CACHE_COUNT
Pair *
newPair( void *name, void *value ) {
        Pair *pair = (Pair *) allocate( &CNCache.ptr );
        pair->name = name;
        pair->value = value;
        return pair; }
void
freePair( Pair *pair ) {
	recycle((void**) pair, &CNCache.ptr ); }

#endif // !CN_CACHE_COUNT
#else // !CN_CACHE

uint64_t cnCacheGetSize( void ) { return 0; }
uint64_t cnCacheGetUsed( void ) { return 0; }


Pair *
newPair( void *name, void *value ) {
	Pair *pair = malloc( sizeof(Pair) );
        pair->name = name;
        pair->value = value;
        return pair; }

void
freePair( Pair *pair ) {
	if (( pair )) free( pair );
	else fprintf( stderr, "***** Error: freePair(): NULL argument\n" ); }

#endif // !CN_CACHE

//---------------------------------------------------------------------------
//	allocate, recycle
//---------------------------------------------------------------------------
#ifdef CN_CACHE

static inline void *
allocate( void ***Cache ) {
	void **this, **cache = *Cache;
	if (( cache )) this = cache;
	else {
		this = malloc( CN_CACHE_BSIZE * 2 * sizeof(void*) );
		this[ 1 ] = NULL;
		for ( int i=CN_CACHE_BSIZE-1; (i--); ) {
			this += 2;
			this[ 1 ] = this-2; }
#ifdef CN_CACHE_COUNT
		CNCache.size += CN_CACHE_BSIZE;
#endif
       	}
 	*Cache = this[ 1 ];
	return this; }

static inline void
recycle( void **item, void ***Cache ) {
	item[ 1 ] = *Cache;
	*Cache = item; }


#endif // CN_CACHE
