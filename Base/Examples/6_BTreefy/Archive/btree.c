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
	if ( !*sequence ) return NULL;

	listItem *stack = NULL;

	int position = POSITION_LEFT, scope = 1;
	BTreeNode *sub = NULL;
	BTreeNode *node = NULL;
	BTreeNode *root = NULL;

	for ( char *p=sequence; *p && scope; p++ ) {
		switch ( *p ) {
		case '(':
			scope++;
			if ( !sub ) {
				sub = newBTree( p );
				if (( root )) {
					addItem( &node->sub[position], sub );
				}
				else root = node = sub;
			}
			addItem( &stack, node );
			add_item( &stack, position );
			node = sub; sub = NULL;
			position = POSITION_LEFT;
			break;
		case ',':
			position = POSITION_RIGHT;
			// no break
		case ':':
			sub = newBTree( p );
			addItem( &node->sub[ position ], sub );
			break;
		case ')':
			scope--;
			if ( !scope ) break;
			reorderListItem( &node->sub[ 0 ] );
			reorderListItem( &node->sub[ 1 ] );
			position = pop_item( &stack );
			node = popListItem( &stack );
			sub = node->sub[ position ]->ptr;
			break;
		default:
			if ( !sub ) {
				sub = newBTree( p );
				if (( root )) {
					addItem( &node->sub[position], sub );
				}
				else root = node = sub;
			}
		}
	}
	freeListItem( &stack );
	reorderListItem( &root->sub[ 0 ] );
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

