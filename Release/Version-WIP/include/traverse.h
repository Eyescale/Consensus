#ifndef TRAVERSE_H
#define TRAVERSE_H

#include "string_util.h"

#define s_append( bgn, end ) \
	for ( char *_c=bgn; _c!=end; StringAppend(s,*_c++) );
#define s_add( str ) \
	for ( char *_c=str; *_c; StringAppend(s,*_c++) );

#define AS_SUB  0
#define SUB     2

//===========================================================================
//	traversal flags
//===========================================================================
#define FIRST		1
#define FILTERED	(1<<1)
#define SUB_EXPR	(1<<2)
#define MARKED		(1<<3)
#define EMARKED		(1<<4)
#define NEGATED		(1<<5)
#define INFORMED	(1<<6)
#define LEVEL		(1<<7)
#define SET		(1<<8)
#define ASSIGN		(1<<9)
#define DOT		(1<<10)
#define COMPOUND	(1<<11)
#define TERNARY		(1<<12)
#define COUPLE		(1<<13)
#define PIPED		(1<<14)
#define LITERAL		(1<<15)
#define CLEAN		(1<<16)
#define LISTABLE	(1<<17)
#define NEW		(1<<18)
#define CARRY		(1<<19)
#define VECTOR		(1<<20)
#define EENOV		(1<<21)
#define ELLIPSIS	(1<<22)

#define are_f( b ) \
	((flags&(b))==(b))
#define is_f( b ) \
	( flags & (b) )
#define f_set( b ) \
	flags |= (b);
#define f_clr( b ) \
	flags &= ~(b);
#define f_xor( b ) \
	flags ^= (b);
#define f_reset( b, save ) \
	flags = (b)|(flags&(save));
#define	f_push( stack ) \
	add_item( stack, flags );
#define	f_pop( stack, save ) \
	flags = pop_item( stack )|(flags&(save));
#define f_cls \
	f_clr(NEGATED) f_set(INFORMED)
#define f_tag( stack, flag ) \
	traverse_tag( flags, stack, flag );

inline void
traverse_tag( int flags, listItem **stack, int flag )
{
	union { void *ptr; int value; } icast;
	switch ( flag ) {
	case MARKED:
	case EMARKED:
		if ( !is_f(LEVEL) ) return;
		for ( listItem *i=*stack; i!=NULL; i=i->next ) {
			icast.ptr = i->ptr;
			icast.value |= flag;
			i->ptr = icast.ptr;
			if (!( icast.value & LEVEL )) break; }
		break;
	case COMPOUND:
		for ( listItem *i=*stack; i!=NULL; i=i->next ) {
			icast.ptr = i->ptr;
			icast.value |= COMPOUND;
			i->ptr = icast.ptr; }
		break; }
}

inline int
f_signal_authorize( listItem **stack )
/*
	Assumption: TERNARY is set all the way
	prevents signal from being used in TERNARY guard, where
		ternary:( guard ?_:_)
	in other words, only the very base level of the current
	flags stack should be tagged FIRST
*/
{
	union { void *ptr; int value; } icast;
	for ( listItem *i=*stack; i!=NULL; i=i->next ) {
		icast.ptr = i->ptr;
		int flags = icast.value;
		if is_f( FIRST ) return !(i->next); }
	return 1;
}

//===========================================================================
//	bm_traverse callbacks
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
static inline void
xpn_add( listItem **xp, int as_sub, int position )
{
	union { int value; void *ptr; } icast;
	icast.value = as_sub + position;
	addItem( xp, icast.ptr );
}
static inline void
xpn_set( listItem *xp, int as_sub, int position )
{
	union { int value; void *ptr; } icast;
	icast.value = as_sub + position;
	xp->ptr = icast.ptr;
}
static inline void
xpn_free( listItem **xp, listItem *level )
{
	while ( *xp!=level ) popListItem( xp );
}
static inline void
xpn_out( FILE *stream, listItem *xp )
{
	union { int value; void *ptr; } icast;
	while ( xp ) {
		icast.ptr = xp->ptr;
		fprintf( stream, "%d", icast.value );
		xp = xp->next; }
}


#endif	// TRAVERSE_H
