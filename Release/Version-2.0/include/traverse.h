#ifndef TRAVERSE_H
#define TRAVERSE_H

#include "string_util.h"

#define AS_SUB  0
#define SUB     2

//===========================================================================
//	traversal flags
//===========================================================================
#define FIRST		1
#define FILTERED	(1<<1)
#define SUB_EXPR	(1<<2)
#define MARKED		(1<<3)
#define NEGATED		(1<<4)
#define INFORMED	(1<<5)
#define LEVEL		(1<<6)
#define SET		(1<<7)
#define ASSIGN		(1<<8)
#define DOT		(1<<9)
#define COMPOUND	(1<<10)
#define TERNARY		(1<<11)
#define COUPLE		(1<<12)
#define PIPED		(1<<13)
#define LITERAL		(1<<14)
#define CLEAN		(1<<15)
#define LISTABLE	(1<<16)
#define NEW		(1<<17)
#define CARRY		(1<<18)
#define VECTOR		(1<<19)
#define EENOV		(1<<20)

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
		if ( !is_f(LEVEL) ) return;
		for ( listItem *i=*stack; i!=NULL; i=i->next ) {
			icast.ptr = i->ptr;
			flags = icast.value;
			icast.value |= MARKED;
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
	Special case: authorize the usage of signal~ as term
	in ternary do ( guard ? term : term ) expression
	Note that stack is pushed (in parser.c)
	  . first upon entering the expression - therefore
	    before the TERNARY flag is set,
	  . and then again only upon encountering a second,
	    third, etc. ternary operator - at which point
	    the FIRST flag is cleared
		do ( guard ? guard ? term : term : term )
		   ^---- 1st push  ^---- 2nd push: FIRST cleared
	We could make the 2nd push on the first ternary operator
	for the very same effect - and we wouldn't have to test
	!next_i below
*/
{
	union { void *ptr; int value; } icast;
	for ( listItem *i=*stack, *next_i; i!=NULL; i=next_i ) {
		next_i = i->next;
		icast.ptr = i->ptr;
		int flags = icast.value;
		if is_f( FIRST ) {
			return ( !next_i || !next_i->next ); } }
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


#endif	// TRAVERSE_H
