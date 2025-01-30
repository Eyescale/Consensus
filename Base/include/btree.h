#ifndef BTREE_H
#define BTREE_H

#include "pair.h"

typedef struct {
	union { void *ptr; uint value[2]; } type; // 64-bit
	Pair *root; } BTree;

typedef void freeBTreeCB( int user_type, int ndx, void *value );

BTree *	newBTree( int user_type );
void	freeBTree( BTree *, freeBTreeCB );
uint	btreeAdd( BTree *, void * );
void *	btreeLookup( BTree *, uint );

#endif // BTREE_H
