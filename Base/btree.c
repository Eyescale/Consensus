#include <stdio.h>
#include <stdlib.h>

#include "pair.h"
#include "btree.h"

//===========================================================================
//	btreefy
//===========================================================================
BTreeNode *
btreefy( char *sequence )
/*
	assumption: sequence is syntactically correct
*/
{
	union { int value; void *ptr; } icast;
	listItem *stack = NULL;

	BTreeNode *root = NULL;
	BTreeNode *node = NULL;
	BTreeNode *sub = NULL;

	int position = POSITION_LEFT, scope = 1;

	for ( char *p=sequence; *p && scope; p++ ) {
		switch ( *p ) {
		case '(':
			scope++;
			if ( sub == NULL ) {
				sub = newBTree( p );
				if (( root )) {
					addItem( &node->sub[ position ], sub );
				}
				else root = node = sub;
			}
			icast.value = position;
			addItem( &stack, node );
			addItem( &stack, icast.ptr );
			node = sub; sub = NULL;
			position = POSITION_LEFT;
			break;
		case ',':
			position = POSITION_RIGHT;
			// no break
		case ':':
			sub = newBTree( p );
			if (( root )) {
				addItem( &node->sub[ position ], sub );
			}
			else root = node = sub;
			break;
		case ')':
			scope--;
			if ( !scope ) break;
			reorderListItem( &node->sub[ 0 ] );
			reorderListItem( &node->sub[ 1 ] );
			position = (int) popListItem( &stack );
			node = popListItem( &stack );
			sub = node->sub[ position ]->ptr;
			break;
		default:
			if ( sub == NULL ) {
				sub = newBTree( p );
				if (( root )) {
					addItem( &node->sub[ position ], sub );
				}
				else root = node = sub;
			}
			break;
		}
	}
	freeListItem( &stack );
	reorderListItem( &root->sub[ 0 ] );
	return root;
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
					icast.value = POSITION_LEFT;
					addItem( &stack, i );
					addItem( &stack, icast.ptr );
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
				as_sub_position = (int) stack->ptr;
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
					position = (int) popListItem( &stack );
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
		position = (int) popListItem( &stack );
		i = popListItem( &stack );
	}
	freeItem( i );
	return 1;
}

//===========================================================================
//	newBTree, freeBTree
//===========================================================================
BTreeNode *
newBTree( void *data )
{
	Pair *sub = newPair( NULL, NULL );
	return (BTreeNode *) newPair( data, sub );
}
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
				icast.value = position;
				addItem( &stack, i );
				addItem( &stack, icast.ptr );
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
				int as_sub_position = (int) stack->ptr;
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
					position = (int) popListItem( &stack );
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

//===========================================================================
//	BT Test executable
//===========================================================================
#ifdef BT_TEST
char *sequence =
	"((titi,*toto:(billy,boy)),(tata:ernest,tutu))"
//	"%(**?)"
	;

static void output_closure( int level, int previous );
static void output_level( int level );
typedef struct {
	int level;
}
OutputData;
static BTreeTraverseCB output_CB;

int
main( int argc, char *argv[] )
{
	if ( argc > 1 ) sequence = argv[ 1 ];
	BTreeNode *btree = btreefy( sequence );
	printf( "Sequence\n\t%s\nTree\n", sequence );
	OutputData data;
	data.level = 0;
	bt_traverse( btree, output_CB, &data );
	output_closure( 1, data.level );
	if ( data.level == 1 ) printf( "\n" );
	freeBTree( btree );
}
static int
output_CB( listItem **path, int position, listItem *sub, void *user_data )
{
	OutputData *data = user_data;

	int level = 0;
	for ( listItem *i=*path; i!=NULL; i=i->next )
		level++;
	output_closure( level, data->level );

	int first = 1;
	BTreeNode *node = (*path)->ptr;
	for ( char *p=node->data; *p; p++ ) {
		switch ( *p ) {
		case '(':
			if ( first ) {
				output_level( level );
			}
			printf( "(\n" );
			goto RETURN;
		case ')':
			printf( "\n" );
			goto RETURN;
		case ':':
			if ( first ) continue;
			else {
				printf( "\n" );
				goto RETURN;
			}
			break;
		case ',':
			if ( first ) {
				output_level( level-1 );
				printf( ",\n" );
			} else {
				printf( "\n" );
				goto RETURN;
			}
			break;
		default:
			if ( first ) {
				first = 0;
				output_level( level );
			}
			printf( "%c", *p );
		}
	}
RETURN:
	data->level = level;
	return BT_CONTINUE;
}
static void
output_closure( int level, int previous )
{
	for ( int i=previous; i > level; i-- ) {
		for ( int j=1; j < i; j++ )
			printf( "\t" );
		printf( ")\n" );
	}
}
static void
output_level( int level )
{
	for ( int i=0; i<level; i++ )
		printf( "\t" );
}
#endif	// TEST
