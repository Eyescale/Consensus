#ifndef PARSER_PRIVATE_H
#define PARSER_PRIVATE_H

#include "traversal_flags.h"

//===========================================================================
//	private parser macros
//===========================================================================
#define f_tag( stack, b ) \
	f_tag_( &flags, stack, (b) );
#define f_parent( b ) \
	f_parent_( stack, (b) )
#define f_restore( b ) \
	f_restore_( (b), &flags, stack );
#define f_markable( stack ) \
	f_markable_( flags, stack )
#define f_signalable( stack ) \
	f_signalable_( flags, stack )
#define byref \
	FORE|EENOV|SUB_EXPR|NEGATED|FILTERED|STARRED

static inline void
f_tag_( int *f, listItem **stack, int flags ) {
	*f |= flags;
	for ( listItem *i=*stack; i!=NULL; i=i->next ) {
		int value = cast_i(i->ptr) | flags;
		i->ptr = cast_ptr( value ); } }

static inline int
f_parent_( listItem **stack, int bits )
/*
	Assumption: *stack != NULL
*/ {
	int flags = cast_i((*stack)->ptr);
	return ( flags & bits ); }

static inline void
f_restore_( int bits, int *f, listItem **stack ) {
	int flags = *f;
	if is_f( LEVEL ) {
		// set flags to parent's value
		*f = (*f&~bits)|f_parent(bits); }
	else {	// clear flags bits
		*f &= ~bits; } }

static inline int
f_markable_( int flags, listItem **stack )
/*
	Authorize '?' as term in possibly ternary expression
	by verifying, in this case, that we have %(_) e.g.
		%( guard ? term : term )
*/ {
	listItem *i, *next_i=*stack;
	if ( !is_f(SUB_EXPR) && is_f(STARRED) ) return 0;
	if is_f( MARKED|NEGATED ) return 0;
	for ( ; ; ) {
		if ( is_f(SUB_EXPR|EENOV) && !is_f(LEVEL) )
			return 1;
		else if are_f( FIRST|TERNARY )
			return 0;
		else if ( !next_i )
			return 1;
		i = next_i;
		next_i = i->next;
		flags = cast_i(i->ptr); } }

static inline int
f_signalable_( int flags, listItem **stack )
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
*/ {
	if ( is_f(ASSIGN) || is_f(byref) ) return 0;
	if ( !is_f(LEVEL) || !is_f(TERNARY) ) return 1;
	for ( listItem *i=*stack, *next_i; i!=NULL; i=next_i ) {
		next_i = i->next;
		int flags = cast_i(i->ptr);
		if is_f( FIRST ) {
			return !next_i->next; } }
	return 1; }

#endif	// PARSER_PRIVATE_H
