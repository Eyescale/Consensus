#include <stdio.h>
#include <stdlib.h>

#include "list.h"
#include "pair.h"
#include "string_util.h"
#include "btree.h"

//===========================================================================
//	btreefy
//===========================================================================
static BTreeNode *newNode( char * );

BTreeNode *
btreefy( char *sequence )
/*
	assumption: sequence is syntactically correct
*/
{
	listItem *stack = NULL;
	BTreeNode *root = newNode( NULL );

	// fake '('
	BTreeNode *node = root;
	int position = POSITION_LEFT;
	BTreeNode *sub = ( *sequence==':' ) ? NULL: newNode( sequence );
	listItem **node_sub = &node->sub[ POSITION_LEFT ];
	char *p = sequence;
	for ( ; *p; p++ ) {
		switch ( *p ) {
		case '{':
		case '(':
			add_item( &stack, position );
			addItem( &stack, node );
			node = sub;
			position = POSITION_LEFT;
			node_sub = &node->sub[ POSITION_LEFT ];
			sub = newNode( p+1 );
			break;
		case '|':
		case ':':
			if ((sub)) addItem( node_sub, sub );
			sub = newNode( p );
			break;
		case ',':
			if ( !stack ) goto RETURN;
			addItem( node_sub, sub );
			if (!( *(char *)node->p == '{' )) {
				reorderListItem( node_sub );
				position = POSITION_RIGHT;
				node_sub = &node->sub[ POSITION_RIGHT ];
			}
			sub = newNode( p );
			break;
		case '}':
		case ')':
			if ( !stack ) goto RETURN;
			addItem( node_sub, sub );
			reorderListItem( node_sub );
			sub = node;
			node = popListItem( &stack );
			position = pop_item( &stack );
			node_sub = &node->sub[ position ];
			break;
		}
	}
RETURN:
	if ((sub)) addItem( &root->sub[ 0 ], sub );
	reorderListItem( &root->sub[0] );
	return root;
}
static BTreeNode *
newNode( char *p )
{
	Pair *sub = newPair( NULL, NULL );
	return (BTreeNode *) newPair( p, sub );
}

//===========================================================================
//	output_btree
//===========================================================================
static void output_data( char *p, int level, int base );
static void output_tab( int level );

void
output_btree( BTreeNode *root, int base )
{
	listItem *stack = NULL;
	int position = POSITION_LEFT;
	listItem *i = root->sub[ POSITION_LEFT ];
	int level = base;
	while (( i )) {
		BTreeNode *node = i->ptr;
		output_data( node->p, level, base );
		listItem *j = node->sub[ position ];
		if (( j )) {
			if ( position==POSITION_LEFT ) {
				addItem( &stack, i );
				add_item( &stack, POSITION_LEFT );
				level++;
			}
			else position = POSITION_LEFT;
			i = j; continue;
		}
		if (( i->next ))
			i = i->next;
		else if (( stack )) {
			union { int value; void *ptr; } icast;
			do {
				icast.ptr = stack->ptr;
				if ( icast.value==POSITION_LEFT ) {
					icast.value = POSITION_RIGHT;
					stack->ptr = icast.ptr;

					listItem *j = stack->next->ptr;
					BTreeNode *parent = j->ptr;

					i = parent->sub[ POSITION_RIGHT ];
				}
				else {
					level--;
					position = pop_item( &stack );
					i = popListItem( &stack );
					i = i->next;
				}
				position = POSITION_LEFT;
			} while ( !i && (stack) );
		}
		else break;
	}
}
static void
output_data( char *p, int level, int base )
/*
   Use Cases
	_	{	(
	_|	_{	_(	_:	_,	_)	_}
	|_	|_{	|_(	|_|	|_:	|_,	|_)	|_}
	:_	:_{	:_(	:_|	:_:	:_,	:_)	:_}
	,_	,_{	,_(	,_|	,_:	,_,	,_)	,_}
*/
{
	switch ( *p ) {
		case '{':
		case '(':
			output_tab( level );
			putchar( *p );
			putchar( '\n' );
			return;
		case ',':
			output_tab( level-1 );
			putchar( ',' );
			putchar( '\n' );
			output_tab( level );
			break;
		case '|':
		case ':':
		default:
			output_tab( level );
			putchar( *p );
			break;
	}
	for ( p++; *p; p++ ) {
		switch ( *p ) {
		case '{':
		case '(':
			putchar( *p );
			putchar( '\n' );
			return;
		case '}':
		case ')':
			do {
				level--;
				putchar( '\n' );
				output_tab( level );
				putchar( *p );
				putchar( '\n' );
			}
			while ((level>base) && ((p[1]=='}'||p[1]==')') ? p++ : NULL ));
			return;
		case '|':
		case ':':
		case ',':
			putchar( '\n' );
			return;
		default:
			putchar( *p );
		}
	}
	// here we finished sequence !
	putchar( '\n' );
}
static void
output_tab( int level )
{
	while ( level-- ) putchar( '\t' );
}

//===========================================================================
//	freeBTree
//===========================================================================
static void freeNode( BTreeNode * );

void
freeBTree( BTreeNode *root )
{
	listItem *stack = NULL;
	int position = POSITION_LEFT;
	listItem *i = root->sub[ position ];
	while (( i )) {
		BTreeNode *node = i->ptr;
		listItem *j = node->sub[ position ];
		if (( j )) {
			if ( position == POSITION_LEFT ) {
				addItem( &stack, i );
				add_item( &stack, POSITION_LEFT );
			}
			position = POSITION_LEFT;
			i = j; continue;
		}
		freeNode( node );
		if (( i->next ))
			i = i->next;
		else if (( stack )) {
			union { int value; void *ptr; } icast;
			do {
				icast.ptr = stack->ptr;
				if ( icast.value==POSITION_LEFT ) {
					icast.value = POSITION_RIGHT;
					stack->ptr = icast.ptr;

					listItem *j = stack->next->ptr;
					BTreeNode *parent = j->ptr;

					i = parent->sub[ POSITION_RIGHT ];
				}
				else {
					position = pop_item( &stack );
					i = popListItem( &stack );
					freeNode( i->ptr );
					i = i->next;
				}
				position = POSITION_LEFT;
			} while ( !i && (stack) );
		}
		else break;
	}
	freeNode( root );
}

static void
freeNode( BTreeNode *node )
{
	freeListItem( &node->sub[ POSITION_LEFT ] );
	freeListItem( &node->sub[ POSITION_RIGHT ] );
	freePair((Pair *) node->sub );
	freePair((Pair *) node );
}
