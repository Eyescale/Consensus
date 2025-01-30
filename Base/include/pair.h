#ifndef PAIR_H
#define PAIR_H

#include <stdint.h>

#define CNMemorySize	cnCacheGetSize()
#define CNMemoryUsed	cnCacheGetUsed()

typedef unsigned int uint;

uint64_t cnCacheGetSize( void );
uint64_t cnCacheGetUsed( void );

typedef struct {
	void *name;
	void *value;
} Pair;

Pair * newPair( void *name, void *value );
Pair * new_pair( int name, int value );
void freePair( Pair *pair );

static inline void * cast_ptr( int i ) {
	union { void *ptr; int value; } icast;
	return (icast.value=i,icast.ptr); }

static inline int cast_i( void *ptr ) {
	union { void *ptr; int value; } icast;
	return (icast.ptr=ptr,icast.value); }

#define new_pair( name, value ) \
	newPair( cast_ptr(name), cast_ptr(value) )


#endif	// PAIR_H
