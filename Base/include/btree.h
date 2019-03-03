#ifndef BTREE_H
#define BTREE_H

#include "list.h"

typedef struct {
	void *data;
	listItem **sub;
} BTreeNode;

#define POSITION_LEFT	0
#define POSITION_RIGHT	1

BTreeNode *btreefy( char *sequence );
BTreeNode *newBTree( void * );
void freeBTree( BTreeNode * );

enum {
	BT_DONE,
	BT_PRUNE,
	BT_CONTINUE
};
typedef int BTreeTraverseCB( listItem **path, int position, listItem *sub, void *user_data );
int bt_traverse( BTreeNode *, BTreeTraverseCB, void *user_data );


#endif // BTREE_H
