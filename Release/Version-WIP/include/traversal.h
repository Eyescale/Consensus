#ifndef TRAVERSAL_H
#define TRAVERSAL_H

#include "database.h"

#define AS_SUB  0
#define SUB     2

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
	BM_PRUNE_TERM,
	BM_PRUNE_LEVEL
	} BMCBTake;

typedef char *BMTraversal( char *, BMTraverseData *, int );
typedef BMCBTake BMTraverseCB( BMTraverseData *, char **p, int flags, int f_next );

//===========================================================================
//	utilities
//===========================================================================
static inline void xpn_add( listItem **xp, int as_sub, int position ) {
	int i = as_sub + position;
	addItem( xp, cast_ptr(i) ); }

static inline void xpn_set( listItem *xp, int as_sub, int position ) {
	int i = as_sub + position;
	xp->ptr = cast_ptr( i ); }

static inline void xpn_free( listItem **xp, listItem *level ) {
	while ( *xp!=level ) popListItem( xp ); }

static inline void xpn_out( FILE *stream, listItem *xp ) {
	while ( xp ) {
		fprintf( stream, "%d", cast_i(xp->ptr) );
		xp = xp->next; } }

static inline listItem * subx( char *p ) {
	if ( *p=='.' ) return NULL;
	listItem *sub = NULL;
	char *s=p;
	for ( int ndx=0; ; )
		switch ( *p++ ) {
		case '(': add_item( &sub, ndx ); ndx=0; break;
		case ',': sub->ptr=cast_ptr( 1 ); break;
		case '?': reorderListItem( &sub ); return sub;
		case '.': break;
		default: fprintf( stderr, ">>>>> B%%: Warning: subx:\n"
			"\t\t_:%s\n\t<<<<< format inconsistent - ignoring\n", s );
			freeListItem( &sub ); return NULL; } }

static inline CNInstance * xsub( CNInstance *x, listItem *xpn ) {
	if ( !xpn ) return x;
	for ( listItem *i=xpn; i!=NULL; i=i->next ) {
		x = CNSUB( x, cast_i(i->ptr) );
		if ( !x ) return NULL; }
	return x; }


#endif	// TRAVERSAL_H
