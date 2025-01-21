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
#define EMARKED		(1<<4)
#define NEGATED		(1<<5)
#define INFORMED	(1<<6)
#define LEVEL		(1<<7)
#define SET		(1<<8)
#define ASSIGN		(1<<9)
#define DOT		(1<<10)
#define PROTECTED	(1<<11)
#define TERNARY		(1<<12)
#define COUPLE		(1<<13)
#define PIPED		(1<<14)
#define LITERAL		(1<<15)
#define CLEAN		(1<<16)
#define LISTABLE	(1<<17)
#define CARRY		(1<<18)
#define VECTOR		(1<<19)
#define EENOV		(1<<20)
#define ELLIPSIS	(1<<21)
#define PRIMED		(1<<22)
#define STARRED		(1<<23)

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
#define f_tag( stack ) \
	f_tag_( &flags, stack );
#define f_ternary_markable( stack ) \
	f_ternary_markable_( flags, stack )
#define f_ternary_signalable( stack ) \
	f_ternary_signalable_( flags, stack )
#define f_restore( b ) \
	f_restore_( (b), &flags, stack );
#define f_actionable( stack ) \
	!is_f(EENOV|SUB_EXPR|NEGATED|FILTERED|STARRED)

inline void
f_tag_( int *flags, listItem **stack )
{
	union { void *ptr; int value; } icast;
	for ( listItem *i=*stack; i!=NULL; i=i->next ) {
		icast.ptr = i->ptr;
		icast.value |= PROTECTED;
		i->ptr = icast.ptr; }
}
inline int
f_parent_marked( listItem **stack )
/*
	Assumption: *stack != NULL
*/
{
	union { void *ptr; int value; } icast;
	listItem *i = *stack;
	icast.ptr = i->ptr;
	int flags = icast.value;
	return is_f( MARKED );
}
inline int
f_ternary_markable_( int flags, listItem **stack )
/*
	Authorize '?' as term in possibly ternary expression
	by verifying, in this case, that we have %(_) e.g.
		%( guard ? term : term )
*/
{
	union { void *ptr; int value; } icast;
	listItem *i, *next_i=*stack;
	for ( ; ; ) {
		if ( is_f(SUB_EXPR|EENOV) && !is_f(LEVEL) )
			return 1;
		else if are_f( FIRST|TERNARY )
			return 0;
		else if ( !next_i )
			return 1;
		i = next_i;
		next_i = i->next;
		icast.ptr = i->ptr;
		flags = icast.value; }
}
inline int
f_ternary_signalable_( int flags, listItem **stack )
/*
	Authorize the usage of signal~ as term in mono-level ternary
	expression do ( guard ? term : term )
	Note that stack is pushed,
	  . first, upon entering the expression - therefore
	    before the TERNARY flag is set,
	  . and then again, upon each ternary operator '?', e.g.
		do ( guard ? guard ? term : term : term )
		   ^---- 1st push 
			   ^---- 2nd push: set TERNARY, clear FIRST
*/
{
	if ( !is_f(TERNARY) ) return 1;
	union { void *ptr; int value; } icast;
	for ( listItem *i=*stack, *next_i; i!=NULL; i=next_i ) {
		next_i = i->next;
		icast.ptr = i->ptr;
		int flags = icast.value;
		if is_f( FIRST ) {
			return !next_i->next; } }
	return 1;
}
inline void
f_restore_( int bits, int *f, listItem **stack )
{
	int flags = *f;
	if is_f( LEVEL ) {
		// set flags to parent's value
		union { void *ptr; int value; } icast;
		icast.ptr = (*stack)->ptr;
		*f = (*f&~bits)|(icast.value&bits); }
	else {	// clear flags bits
		*f &= ~bits; }
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
