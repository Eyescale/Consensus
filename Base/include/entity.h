#ifndef ENTITY_H
#define ENTITY_H

#include "list.h"

typedef struct _Entity {
	struct _Entity **sub;
	listItem **as_sub;
} CNEntity;

CNEntity *cn_new( CNEntity *, CNEntity * );
CNEntity *cn_instance( CNEntity *, CNEntity *, const int pivot );
void cn_rewire( CNEntity *, int ndx, CNEntity *sub );
void cn_prune( CNEntity * );
void cn_release( CNEntity * );
void cn_free( CNEntity * );

static inline CNEntity *cn_carrier( Pair *sub ) {
	Pair *as_sub = newPair( NULL, NULL );
	return (CNEntity*) newPair( sub, as_sub ); }

//---------------------------------------------------------------------------
//	exponent utilities
//---------------------------------------------------------------------------
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

//---------------------------------------------------------------------------
//	is_xpn_expr, xpn_make, xsub
//---------------------------------------------------------------------------
/*
	sub format expressions
*/
#define sub_case( event, test ) \
	case event: if (!test) { freeListItem(&stack); return 0; } else

static inline int is_xpn_expr( char *p ) {
	if ( *p++!='(' ) return 0;
	int first, informed, marked;
	listItem *stack = NULL;
	informed = marked = 0;
	first = 1;
	for ( ; ; )
		switch ( *p++ ) {
		sub_case( '(', !(informed||marked) ) {
			add_item(&stack,first);
			first = 1;
			break; }
		sub_case( ')', (informed&&!first) ) {
			if ( !stack ) return 1;
			first = pop_item(&stack);
			break; }
		sub_case( ',', (first&&informed) ) {
			first = informed = 0;
			break; }
		sub_case( '.', ((first||marked)&&!informed) ) { 
			informed = 1;
			break; }
		sub_case( '?', !(informed||marked) ) {
			informed = marked = 1;
			break; }
		default:
			freeListItem( &stack );
			return 0; } }

static inline listItem * xpn_make( char *p ) {
	if ( !is_xpn_expr( p ) ) return NULL;
	listItem *sub = NULL;
	int ndx = 0;
	for ( ; ; ) switch ( *p++ ) {
		case '(': add_item( &sub, ndx ); ndx=0; break;
		case ',': ndx=1; sub->ptr=cast_ptr(ndx); break;
		case '?': reorderListItem( &sub ); return sub; } }


#endif	// ENTITY_H
