#ifndef PAIR_H
#define PAIR_H

typedef struct {
	void *name;
	void *value;
} Pair;

Pair * newPair( void *name, void *value );
Pair * new_pair( int name, int value );
void freePair( Pair *pair );



#endif	// PAIR_H
