#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "wildcard.h"

static BTreeTraverseCB find_path_CB;
static BTreeTraverseCB prove_wild_CB;
static BTreeTraverseCB find_peer_CB;

typedef struct {
	char *p;
	int marked;
	int success;
	listItem *path;
	int position;
	void *ref;
} TraverseData;

//===========================================================================
//	wildcard_opt	- optimizes bm_verify()
//===========================================================================
/* Optimizes bm_verify - cf expression.c
   Evaluates the following conditions for a wildcard candidate:
	. Firstly, if it's a mark, that it is not starred
	. Secondly that it is neither base nor filtered - that is, if it
	  belongs to a filtered expression, then it should be the last term
	  of that expression, and in that case all the other filter terms
	  must be wildcards.
	. Thirdly, that it is the second term of a couple.
	. Fourthly, that the first term of that couple is also a wildcard.
	. Lastly, that if either term of that couple is marked, then that
	  the sub-expression to which the couple pertains does not contain
	  other couples or non-wildcards terms.
   Failing any of these conditions (with the exception of the first) will
   cause wildcard_opt() to return 1 - thereby allowing bm_verify() to bypass
   any further inquiry on the wildcard, which can result in substantial
   performance optimization depending on the CNDB size.
   Otherwise, that is: either failing the first condition or meeting all
   of the others, wildcard_opt() will return 0, requiring bm_verify() to
   perform a full evaluation on the wildcard.
*/
static char *term_start( char *, int * );

int
wildcard_opt( char *p, BTreeNode *root )
/*
	Assumptions:
	. the passed sentence is syntactically correct
	. the passed p does belong to sentence
*/
{
#ifdef DEBUG
fprintf( stderr, "wildcard_opt: IN - '%c'\n", *p );
#endif
	// find path to p in btree
	TraverseData data;
	data.marked = ( *p=='?' );
	data.success = 0;
	data.p = p;
	data.path = NULL;
	data.ref = NULL;
	bt_traverse( root, find_path_CB, &data );
	if ( !data.success ) goto RETURN;
#ifdef DEBUG
fprintf( stderr, "wildcard_opt: path found - next: not starred mark\n" );
#endif
	// check not starred mark
	int starred = 0;
	if ( data.marked ) {
		for ( listItem *i=data.path; i!=NULL; i=i->next ) {
			BTreeNode *n = i->ptr;
			char *q = term_start( n->data, &starred );
			if ( *q=='%' ) break;
		}
		if ( starred ) {
			data.success = 0;
			goto RETURN;
		}
	}
#ifdef DEBUG
fprintf( stderr, "wildcard_opt: not starred mark checked - next: not base\n" );
#endif
	// check not base
	if ( data.path->next == NULL ) goto RETURN;
#ifdef DEBUG
fprintf( stderr, "wildcard_opt: not base checked - next: last in filter\n" );
#endif
	// check last in filter
	listItem *sub = data.ref;
	if (( sub->next )) goto RETURN;
#ifdef DEBUG
fprintf( stderr, "wildcard_opt: last in filter checked - next: wildcard filter\n" );
#endif
	listItem *pn = data.path->next;
	BTreeNode *parent = pn->ptr;
	for ( listItem *i=parent->sub[ data.position ]; (i) && i!=sub; i=i->next ) {
		bt_traverse( i->ptr, prove_wild_CB, &data );
		if ( !data.success ) {
			data.success = 1;
			goto RETURN;
		}
	}
#ifdef DEBUG
fprintf( stderr, "wildcard_opt: wildcard filter checked - next: right position\n" );
#endif
	// check RIGHT position in couple
	pn = data.path;
	for ( listItem *i=data.path->next; i!=NULL; pn=i, i=i->next ) {
		parent = i->ptr;
		if ( parent->sub[ POSITION_RIGHT ] == NULL )
			continue;
		if ( lookupItem( parent->sub[ POSITION_LEFT ], pn->ptr ) )
			goto RETURN;
		if ( lookupItem( parent->sub[ POSITION_RIGHT ], pn->ptr ) )
			break;
	}
#ifdef DEBUG
fprintf( stderr, "wildcard_opt: right position checked - next: wildcard couple\n" );
#endif
	// note that pn->next should hold the first parent *couple* at this point
	// check not singleton all the way
	if (!( pn = pn->next )) goto RETURN;

	// check that couple's first term is wildcard
	parent = pn->ptr;
	for ( listItem *i=parent->sub[ POSITION_LEFT ]; i!=NULL; i=i->next ) {
		bt_traverse( i->ptr, prove_wild_CB, &data );
		if ( !data.success ) {
			data.success = 1;
			goto RETURN;
		}
	}
#ifdef DEBUG
fprintf( stderr, "wildcard_opt: wildcard couple checked - next: marked couple\n" );
#endif
	// if couple is marked, detect other couples or non-wildcard terms
	// in parent expression
	data.success = 0;
	if ( data.marked ) {
		char *q = term_start( parent->data, &starred );
		if ( *q!='%' ) {
			data.ref = parent;
			do {
				pn=pn->next;
				parent = pn->ptr;
				q = term_start( parent->data, &starred );
			} while ( *q!='%' );
			bt_traverse( parent, find_peer_CB, &data );
		}
	}
#ifdef DEBUG
fprintf( stderr, "wildcard_opt: marked couple checked - next: OUT\n" );
#endif

RETURN:
#ifdef DEBUG
fprintf( stderr, "wildcard_opt: OUT - %d\n", data.success );
#endif
	freeListItem( &data.path );
	return data.success;
}
static int
find_path_CB( listItem **path, int position, listItem *sub, void *user_data )
/*
	returns path leading to given wildcard term in btree-expression
	note that the passed (*path)->ptr node is the passed sub->ptr
	in parent->sub[ position ], where parent = (*path)->next->ptr
*/
{
	TraverseData *data = user_data;
	int starred = 0;
	BTreeNode *node = (*path)->ptr;
	char *p = term_start( node->data, &starred );
	switch ( *p ) {
	case '%':
	case '(':
		break;
	default:
		if ( p == data->p ) {
			data->success = 1;
			data->path = *path;
			data->position = position;
			data->ref = sub;
			*path = NULL;
			return BT_DONE;
		}
	}
	return BT_CONTINUE;
}
static int
prove_wild_CB( listItem **path, int position, listItem *sub, void *user_data )
/*
	detects mark, couples or non-wildcard terms in btree
	assumption: data->success is set to 1
*/
{
	TraverseData *data = user_data;
	BTreeNode *node = (*path)->ptr;
	int starred = 0;
	char *p = term_start( node->data, &starred );
	if ( starred ) {	// found non-wildcard term
		data->success = 0;
		return BT_DONE;
	}
	switch ( *p ) {
	case '%':
	case '(': // check couple
		if (( node->sub[ 0 ] ) && ( node->sub[ 1 ] )) {
			data->success = 0;
			return BT_DONE;
		}
		break;
	case '?': // found mark - check not in sub-expression
		for ( listItem *i=*path; i!=NULL; i=i->next ) {
			BTreeNode *n = i->ptr;
			char *q = term_start( n->data, &starred );
			if ( *q=='%' ) return BT_CONTINUE;
		}
		data->marked = 1;
		break;
	case '.':
		break;
	default: // found non-wildcard term
		data->success = 0;
		return BT_DONE;
	}
	return BT_CONTINUE;
}
static int
find_peer_CB( listItem **path, int position, listItem *sub, void *user_data )
/*
	detects couples or non-wildcard terms in btree
	assumption: data->success is set to 0
*/
{
	TraverseData *data = user_data;
	BTreeNode *node = (*path)->ptr;
	if ( node == data->ref )
		return BT_PRUNE;
	if ( lookupItem( data->path, node ) )
		return BT_CONTINUE;
	int starred = 0;
	char *p = term_start( node->data, &starred );
	if ( starred ) {	// found non-wildcard term
		data->success = 1;
		return BT_DONE;
	}
	switch ( *p ) {
	case '%':
	case '(': // found peer couple
		if (( node->sub[ 0 ] ) && ( node->sub[ 1 ] )) {
			data->success = 1;
			return BT_DONE;
		}
		break;
	case '?':
	case '.':
		break;
	default: // found non-wildcard term
		data->success = 1;
		return BT_DONE;
	}
	return BT_CONTINUE;
}
static char *
term_start( char *p, int *starred )
{
	if ( *p==':' || *p==',' ) p++;
	for ( ; ; ) {
		if ( *p=='*' ) *starred = 1;
		else if ( *p!='~' ) break;
		p++;
	}
	return p;
}

