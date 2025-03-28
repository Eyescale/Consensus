#ifndef TRAVERSAL_FLAGS_H
#define TRAVERSAL_FLAGS_H

#define byref \
	FORE|SUB_EXPR|SEL_EXPR|BYREF
#define f_set_BYREF \
	if ( !is_f(FILTERED) ) f_set( BYREF )

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
#define FORE		(1<<24)
#define PROPPED		(1<<25)
#define CONTRA		(1<<26)
#define BYREF		(1<<27)
#define SEL_EXPR	(1<<28)

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
	f_clr( NEGATED ) \
	f_set( INFORMED )


#endif	// TRAVERSAL_FLAGS_H
