#ifndef BTREE_H
#define BTREE_H

#include "pair.h"

typedef struct {
	union { void *ptr; uint key[2]; } type; // 64-bit
	Pair *root; } BTree;

typedef int freeBTreeCB( uint key[2], void *value );
typedef int shakeBTreeCB( uint key[2], void *value, void *user_data );

BTree *	newBTree( uint user_type );
void	freeBTree( BTree *, freeBTreeCB );
uint	btreeAdd( BTree *, void * );
void *	btreePick( BTree *, uint key[2] );
int	btreeShake( BTree *, shakeBTreeCB, void * );

#endif // BTREE_H
