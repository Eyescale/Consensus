#ifndef PARSER_PRIVATE_H
#define PARSER_PRIVATE_H

#include "parser_flags.h"

// remove leading '.' in ".:func" on '('
#define S_HACK \
	if is_f( INFORMED ) { \
		listItem **s = (listItem **) &data->string->data; \
		reorderListItem( s ); \
		popListItem( s ); \
		reorderListItem( s ); }

// these flags characterize expression as a whole
#define EXPR		1
#define CONTRARY	(1<<1)
#define RELEASED	(1<<2)
#define NASCENT		(1<<3)
#define P_CHAR		(1<<4)
#define P_REGEX		(1<<5)
#define P_LITERAL	(1<<6)
#define P_EENOV		(1<<7)
#define P_STRING	(1<<8)

#define expr(a)		(data->expr&(a))
#define expr_set(a)	data->expr|=(a);
#define expr_clr(a)	data->expr&=~(a);

static BMParseFunc
	bm_parse_string, bm_parse_char, bm_parse_regex,
	bm_parse_eenov, bm_parse_seq;

//===========================================================================
//	private parser macros
//===========================================================================
#define t_take \
	StringAppend( t, event );

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
#define f_contrariable( stack ) \
	f_contrariable_( flags, stack )
#define f_outputable( stack ) \
	f_outputable_( flags, stack )

static inline void
f_tag_( int *f, listItem **stack, int flags ) {
	*f |= flags;
	for ( listItem *i=*stack; i!=NULL; i=i->next ) {
		int value = cast_i(i->ptr) | flags;
		i->ptr = cast_ptr( value ); } }

static inline int
f_parent_( listItem **stack, int bits ) {
	if ( *stack ) {
		int flags = cast_i((*stack)->ptr);
		return ( flags & bits ); }
	return 0; }

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
	if ( is_f(MARKED|NEGATED|CONTRA) || ( is_f(STARRED) && !is_f(SUB_EXPR) ))
		return 0;
	listItem *i, *next_i=*stack;
	for ( ; ; ) {
		if ( is_f(SUB_EXPR) && !is_f(LEVEL) )
			return 1;
		else if are_f( FIRST|TERNARY )
			return 0;
		else if ( !next_i )
			return 1;
		i = next_i;
		next_i = i->next;
		flags = cast_i(i->ptr); } }

static inline int is_base_ternary_term( listItem *, int );
static inline int
f_signalable_( int flags, listItem **stack )
/*
	Authorize the usage of signal~ as term in mono-level ternary
	expression do ( guard ? term : term )
*/ {
	if ( is_f(ASSIGN) || is_f(byref) ) return 0;
	if ( !is_f(LEVEL) || !is_f(TERNARY) ) return 1;
	return is_base_ternary_term( *stack, 0 ); }

static inline int
f_contrariable_( int flags, listItem **stack )
/*
	Authorize the usage of contra term (~.:_) at base level only or
	in mono-level ternary expression do ( guard ? term : term ) 
*/ {
	if ( !is_f(LEVEL|FIRST) || is_f(INFORMED|ASSIGN|TERNARY|byref) )
		return 0; // LEVEL here is the contra level
	listItem *i = *stack;
	listItem *next_i = i->next;
	if (( next_i )) {
		flags = cast_i(i->ptr);
		if is_f( TERNARY )
			return is_base_ternary_term( next_i, 0 );
		else return 0; }
	return 1; }

static inline int
f_outputable_( int flags, listItem **stack )
/*
	Authorize the usage of "_" term at base level only or in
	mono-level ternary expression do ( guard ? term : term ) 
	possibly in vector - Assumption: we have expr( OUTPUT )
*/ {
	if is_f( INFORMED|ASSIGN|byref )
		return 0;
	else if is_f( TERNARY )
		return is_base_ternary_term( *stack, VECTOR );
	else if (( *stack ))
		return ( is_f(VECTOR) && !(*stack)->next );
	return 1; }

static inline int
is_base_ternary_term( listItem *stack, int extra )
/*
	return 1 if stack represents term of mono-level ternary
	expression do ( guard ? term : term ) 
	Note that stack is pushed,
	  . first, upon entering the expression - therefore
	    before the TERNARY flag is set,
	  . and then again, upon each ternary operator '?', e.g.
		do ( guard ? guard ? term : term : term )
		   ^---- 1st push 
			   ^---- 2nd push: set TERNARY, clear FIRST
*/ {
	// Assumption: TERNARY verified beforehand
	for ( listItem *i=stack, *next_i; i!=NULL; i=next_i ) {
		next_i = i->next;
		int flags = cast_i(i->ptr);
		if is_f( FIRST ) {
			if ( !next_i->next ) return 1;
			else if ( extra ) {
				i = next_i;
				flags = cast_i(i->ptr);
				return ( is_f(extra) && !i->next->next ); }
			else return 0; } }
	return 1; }

//===========================================================================
//	PARSER_BGN / PARSER_END
//===========================================================================
#define BM_PARSE_FUNC( func ) \
char *func( int event, BMParseMode mode, BMParseData *data, BMParseCB cb )

#define PARSER_BGN( parser ) \
	CNIO *io = data->io;	\
	listItem **	stack	= &data->stack.flags;	\
	CNString *	s	= data->string;		\
	CNString *	t	= data->txt;		\
	int *		tab	= data->tab;		\
	int *		type	= &data->type;		\
	char *		state	= data->state;		\
	int		flags	= data->flags;		\
	int		errnum	= 0;			\
	int		line	= io->line;		\
	int		column	= io->column;		\
	BMParseBegin( parser, state, event, line, column )

#define PARSER_END \
	BMParseEnd		\
	data->errnum = errnum;	\
	data->flags = flags;	\
	if ( errnum ) bm_parse_report( data, mode, line, column ); \
	return state;

#define PARSER_DEFAULT_END \
	BMParseDefault	\
		on_( EOF )		errnum = ErrUnexpectedEOF;	\
		in_none_sofar		errnum = ErrUnknownState;	\
					data->state = state;		\
		in_other bgn_						\
			on_( '\n' )	errnum = ErrUnexpectedCR;	\
			on_( ' ' )	errnum = ErrUnexpectedSpace;	\
			on_other	errnum = ErrSyntaxError;	\
			end						\
	PARSER_END


#endif	// PARSER_PRIVATE_H
