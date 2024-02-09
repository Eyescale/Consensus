#ifndef PARSER_PRIVATE_H
#define PARSER_PRIVATE_H

#include "traversal_flags.h"

#define byref \
	FORE|SUB_EXPR|NEGATED|FILTERED|STARRED

// these flags characterize expression as a whole
#define CONTRARY	(1<<1)
#define RELEASED	(1<<2)
#define NEWBORN		(1<<3) // cf. WarnNeverLoop
#define P_CHAR		(1<<4)
#define P_REGEX		(1<<5)
#define P_EENOV		(1<<6)
#define P_LITERAL	(1<<7)

#define expr(a)		(data->expr&(a))
#define push_expr(a)	data->expr|=(a);
#define pop_expr(a)	data->expr&=~(a);

static BMParseFunc bm_parse_char, bm_parse_regex, bm_parse_eenov, bm_parse_seq;

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
	if ( is_f(MARKED|NEGATED) ||( is_f(STARRED) && !is_f(SUB_EXPR) ))
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

//===========================================================================
//	PARSER_BGN / PARSER_END
//===========================================================================
#define BM_PARSE_FUNC( func ) \
char *func( int event, BMParseMode mode, BMParseData *data, BMParseCB cb )

#define PARSER_BGN( parser ) \
	CNIO *io = data->io;	\
	listItem **	stack	= &data->stack.flags;	\
	CNString *	s	= data->string;		\
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
