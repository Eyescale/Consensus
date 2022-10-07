#include <stdio.h>
#include <stdlib.h>

#include "list.h"
#include "pair.h"
#include "btree.h"

//===========================================================================
//	btreefy
//===========================================================================
static BTreeNode * newBTree( void * );

BTreeNode *
btreefy( char *sequence )
/*
	assumption: sequence is syntactically correct
*/
{
	listItem *stack = NULL;
	BTreeNode *root = newBTree( NULL );

	// fake '('
	BTreeNode *node = root;
	int position = POSITION_LEFT;
	BTreeNode *sub = ( *sequence==':' ) ? NULL: newBTree( sequence );

	for ( char *p=sequence; *p; p++ ) {
		switch ( *p ) {
		case '{':
		case '(':
			addItem( &stack, sub );
			addItem( &stack, node );
			add_item( &stack, position );
			node = sub;
			position = POSITION_LEFT;
			sub = newBTree( p+1 );
			break;
		case ',':
			if ( !stack ) goto RETURN;
			addItem( &node->sub[ position ], sub );
			if (!( *(char *)node->data == '{' )) {
				reorderListItem( &node->sub[ 0 ] );
				position = POSITION_RIGHT;
			}
			sub = newBTree( p );
			break;
		case '|':
		case ':':
			if ((sub)) addItem( &node->sub[ position ], sub );
			sub = newBTree( p );
			break;
		case '}':
		case ')':
			if ( !stack ) goto RETURN;
			addItem( &node->sub[ position ], sub );
			reorderListItem( &node->sub[ position ] );
			position = pop_item( &stack );
			node = popListItem( &stack );
			sub = popListItem( &stack );
			break;
		}
	}
RETURN:
	if ((sub)) addItem( &root->sub[ 0 ], sub );
	reorderListItem( &root->sub[0] );
	return root;
}
static BTreeNode *
newBTree( void *data )
{
	Pair *sub = newPair( NULL, NULL );
	return (BTreeNode *) newPair( data, sub );
}

//===========================================================================
//	bt_traverse
//===========================================================================
int
bt_traverse( BTreeNode *node, BTreeTraverseCB callback, void *user_data )
{
	if ( node == NULL ) return 0;

	union { int value; void *ptr; } icast;
	listItem *stack = NULL, *path = NULL;

	int	position = POSITION_LEFT,
		as_sub_position = 0,	//  N/A
		prune = 0;

	listItem *i = newItem( node );
	for ( ; ; ) {
		node = i->ptr;
		addItem( &path, node );
		if ( callback ) {
			switch ( callback( &path, as_sub_position, i, user_data ) ) {
			case BT_DONE: goto RETURN;
			case BT_PRUNE: prune = 1;
			}
		}
		if ( prune ) prune = 0;
		else {
			listItem *j = node->sub[ position ];
			if (( j )) {
				as_sub_position = position;
				if ( position == POSITION_LEFT ) {
					addItem( &stack, i );
					add_item( &stack, POSITION_LEFT );
				}
				i = j;
				position = POSITION_LEFT;
				continue;
			}
		}
		for ( ; ; ) {
			if (( node )) popListItem( &path );
			if (( i->next )) {
				i = i->next;
				break;
			}
			else if (( stack )) {
				union { int value; void *ptr; } icast;
				icast.ptr = stack->ptr;
				as_sub_position = icast.value;
				if ( as_sub_position == POSITION_LEFT ) {
					icast.value = POSITION_RIGHT;
					stack->ptr = icast.ptr;
					listItem *k = stack->next->ptr;
					BTreeNode *parent = k->ptr;
					listItem *j = parent->sub[ POSITION_RIGHT ];
					if (( j )) {
						i = j;
						as_sub_position = POSITION_RIGHT;
						position = POSITION_LEFT;
						break;
					}
					else node = NULL;
				}
				else {
					position = pop_item( &stack );
					i = popListItem( &stack );
					node = i->ptr;
				}
			}
			else {
				freeItem( i );
				return 0;
			}
		}
	}
RETURN:
	freeListItem( &path );
	while (( stack )) {
		position = pop_item( &stack );
		i = popListItem( &stack );
	}
	freeItem( i );
	return 1;
}

//===========================================================================
//	freeBTree
//===========================================================================
void
freeBTree( BTreeNode *node )
{
	if ( node == NULL ) return;

	union { int value; void *ptr; } icast;
	listItem *stack = NULL;

	int	position = POSITION_LEFT;

	listItem *i = newItem( node );
	for ( ; ; ) {
		node = i->ptr;
		listItem *j = node->sub[ position ];
		if (( j )) {
			if ( position == POSITION_LEFT ) {
				addItem( &stack, i );
				add_item( &stack, position );
			}
			i = j;
			position = POSITION_LEFT;
			continue;
		}
		for ( ; ; ) {
			if (( node )) {
				freeListItem( &node->sub[ POSITION_LEFT ] );
				freeListItem( &node->sub[ POSITION_RIGHT ] );
				freePair((Pair *) node->sub );
				freePair((Pair *) node );
			}
			if (( i->next )) {
				i = i->next;
				break;
			}
			else if (( stack )) {
				union { int value; void *ptr; } icast;
				icast.ptr = stack->ptr;
				int as_sub_position = icast.value;
				if ( as_sub_position == POSITION_LEFT ) {
					icast.value = POSITION_RIGHT;
					stack->ptr = icast.ptr;
					listItem *k = stack->next->ptr;
					BTreeNode *as_sub = k->ptr;
					listItem *j = as_sub->sub[ POSITION_RIGHT ];
					if (( j )) {
						i = j;
						position = POSITION_LEFT;
						break;
					}
					else node = NULL;
				}
				else {
					position = pop_item( &stack );
					i = popListItem( &stack );
					node = i->ptr;
				}
			}
			else {
				freeItem( i );
				return;
			}
		}
	}
}

