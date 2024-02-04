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

#define subpass_( event, test ) \
	case event: if (!test) { freeListItem(&stack); return 0; } else

static inline int subpass( char *p ) {
	if ( *p++!='(' ) return 0;
	int first, informed, marked, level;
	listItem *stack = NULL;
	informed = marked = 0;
	first = level = 1;
	for ( ; ; )
		switch ( *p++ ) {
		subpass_( '(', !(informed||marked) ) {
			level++; add_item(&stack,first);
			first = 1;
			break; }
		subpass_( ')', (informed&&!first) ) {
			level--; first=pop_item(&stack);
			if ( !level ) return 1;
			break; }
		subpass_( ',', (first&&informed) ) {
			first = informed = 0;
			break; }
		subpass_( '.', ((first||marked)&&!informed) ) {
			informed = 1;
			break; }
		subpass_( '?', !(informed||marked) ) {
			informed = marked = 1;
			break; }
		default:
			freeListItem( &stack );
			return 0; } }

static inline listItem * subx( char *p ) {
	if ( !subpass(p) ) return NULL;
	listItem *sub = NULL;
	int ndx = 0;
	for ( ; ; ) switch ( *p++ ) {
		case '(': add_item( &sub, ndx ); ndx=0; break;
		case ',': sub->ptr=cast_ptr( 1 ); break;
		case '?': reorderListItem( &sub ); return sub; } }

static inline CNInstance * xsub( CNInstance *x, listItem *xpn ) {
	if ( !xpn ) return x;
	for ( listItem *i=xpn; i!=NULL; i=i->next ) {
		x = CNSUB( x, cast_i(i->ptr) );
		if ( !x ) return NULL; }
	return x; }


#endif	// TRAVERSAL_H
