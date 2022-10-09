#ifndef BTREE_H
#define BTREE_H

#include "list.h"

typedef struct {
	char *p;
	listItem **sub;
} BTreeNode;

#define POSITION_LEFT	0
#define POSITION_RIGHT	1

BTreeNode *btreefy( char *sequence );
void output_btree( BTreeNode *, int );
enum {
	BT_DONE,
	BT_PRUNE,
	BT_CONTINUE
};
typedef int BTreeTraverseCB( listItem **path, int position, listItem *sub, void *user_data );
int	bt_traverse( BTreeNode *, BTreeTraverseCB, void *user_data );
void	freeBTree( BTreeNode * );


#endif // BTREE_H
