#ifndef TRAVERSAL_H
#define TRAVERSAL_H

#include "list.h"

//===========================================================================
//	custom user_traversal interface
//===========================================================================
#define BMTraverseError -1
typedef struct {
	void *user_data;
	listItem **stack;
	int flags, done;
	char *p;
	} BMTraverseData;

typedef enum {
	BM_CONTINUE = 0,
	BM_DONE,
	BM_PRUNE_FILTER,
	BM_PRUNE_TERM
	} BMCBTake;

typedef char *BMTraversal( char *, BMTraverseData *, int );
typedef BMCBTake BMTraverseCB( BMTraverseData *, char **p, int flags, int f_next );

//===========================================================================
//	utilities
//===========================================================================
#define AS_SUB  0
#define SUB     2

inline void xpn_add( listItem **xp, int as_sub, int position ) {
	union { int value; void *ptr; } icast;
	icast.value = as_sub + position;
	addItem( xp, icast.ptr ); }

inline void xpn_set( listItem *xp, int as_sub, int position ) {
	union { int value; void *ptr; } icast;
	icast.value = as_sub + position;
	xp->ptr = icast.ptr; }

inline void xpn_free( listItem **xp, listItem *level ) {
	while ( *xp!=level ) popListItem( xp ); }

inline void xpn_out( FILE *stream, listItem *xp ) {
	union { int value; void *ptr; } icast;
	while ( xp ) {
		icast.ptr = xp->ptr;
		fprintf( stream, "%d", icast.value );
		xp = xp->next; } }


#endif	// TRAVERSAL_H
