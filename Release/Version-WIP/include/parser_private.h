#ifndef PARSER_PRIVATE_H
#define PARSER_PRIVATE_H

#include "traversal_flags.h"

//===========================================================================
//	private parser macros
//===========================================================================
#define f_tag( stack, b ) \
	f_tag_( &flags, stack, (b) );
#define f_restore( b ) \
	f_restore_( (b), &flags, stack );
#define f_parent( b ) \
	f_parent_( stack, (b) )
#define f_ternary_markable( stack ) \
	f_ternary_markable_( flags, stack )
#define f_ternary_signalable( stack ) \
	f_ternary_signalable_( flags, stack )
#define f_actionable( stack ) \
	!is_f(EENOV|SUB_EXPR|NEGATED|FILTERED|STARRED)

static inline void
f_tag_( int *f, listItem **stack, int flags ) {
	*f |= flags;
	union { void *ptr; int value; } icast;
	for ( listItem *i=*stack; i!=NULL; i=i->next ) {
		icast.ptr = i->ptr;
		icast.value |= flags;
		i->ptr = icast.ptr; } }

static inline void
f_restore_( int bits, int *f, listItem **stack ) {
	int flags = *f;
	if is_f( LEVEL ) {
		// set flags to parent's value
		union { void *ptr; int value; } icast;
		icast.ptr = (*stack)->ptr;
		*f = (*f&~bits)|(icast.value&bits); }
	else {	// clear flags bits
		*f &= ~bits; } }

static inline int
f_parent_( listItem **stack, int flags )
/*
	Assumption: *stack != NULL
*/ {
	union { void *ptr; int value; } icast;
	icast.ptr = (*stack)->ptr;
	return ( icast.value & flags ); }

static inline int
f_ternary_markable_( int flags, listItem **stack )
/*
	Authorize '?' as term in possibly ternary expression
	by verifying, in this case, that we have %(_) e.g.
		%( guard ? term : term )
*/ {
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
		flags = icast.value; } }

static inline int
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
*/ {
	if ( !is_f(TERNARY) ) return 1;
	union { void *ptr; int value; } icast;
	for ( listItem *i=*stack, *next_i; i!=NULL; i=next_i ) {
		next_i = i->next;
		icast.ptr = i->ptr;
		int flags = icast.value;
		if is_f( FIRST ) {
			return !next_i->next; } }
	return 1; }

#endif	// PARSER_PRIVATE_H
