#ifndef BTREE_H
#define BTREE_H

#include "pair.h"

#define BT_DONE		0
#define BT_CONTINUE	1
#define BT_CACHE	2

typedef struct {
	union { void *ptr; uint key[2]; } type; // 64-bit
	Pair *root;
	} BTreeData;

typedef struct {
	listItem *cache;
	BTreeData *data;
	} BTree;

typedef int freeBTreeCB( uint key[2], void *value );
typedef int btreeShakeCB( uint key[2], void *value, void *user_data );

BTree *	newBTree( uint user_type );
void	freeBTree( BTree *, freeBTreeCB );
uint	btreeAdd( BTree *, void * );
int	btreeShake( BTree *, btreeShakeCB, void * );
void *	btreePick( BTree *, uint key[2] );
void	btreePluck( BTree *, uint key[2], btreeShakeCB, void * );

#endif // BTREE_H
