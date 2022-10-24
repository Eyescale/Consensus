#ifndef TRAVERSE_H
#define TRAVERSE_H

#include "list.h"

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
#define VECTOR		(1<<18)

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
			if (!( icast.value & LEVEL )) break;
		}
		break;
	case COMPOUND:
		for ( listItem *i=*stack; i!=NULL; i=i->next ) {
			icast.ptr = i->ptr;
			icast.value |= COMPOUND;
			i->ptr = icast.ptr;
		}
		break;
	}
}

//===========================================================================
//	traverse callbacks
//===========================================================================
typedef enum {
	BMTermCB = 0,
	BMNotCB,		// ~
	BMBgnSetCB,		// {
	BMEndSetCB,		// }
	BMBgnPipeCB,		// |
	BMModCharacterCB, 	// % followed by one of ,:)}|
	BMRegisterVariableCB,	// %? or %! or ..
	BMSubExpressionCB,	// %(
	BMStarCharacterCB,	// * followed by one of ,:)}|
	BMDereferenceCB,	// *
	BMLiteralCB,		// (:
	BMListCB,		// ...):_sequence_:)
	BMOpenCB,		// (
	BMCloseCB,		// )
	BMEndPipeCB,		// )
	BMDecoupleCB,		// ,
	BMFilterCB,		// :
	BMTernaryOperatorCB,	// informed ?
	BMWildCardCB,		// ? or .
	BMDotExpressionCB,	// .(
	BMDotIdentifierCB,	// .identifier
	BMFormatCB,		// "
	BMCharacterCB,		// '
	BMRegexCB,		// /
	BMIdentifierCB,		// identifier
	BMSignalCB,		// identifier~
	BMCBNum
} BMCBName;
typedef struct {
	void *user_data;
	listItem **stack;
	void *table[ BMCBNum ];
	int flags, done;
	char *p;
} BMTraverseData;
typedef enum {
	BM_CONTINUE = 0,
	BM_DONE,
	BM_PRUNE_FILTER,
	BM_PRUNE_TERM,
	BM_PRUNE_LIST,
	BM_PRUNE_LITERAL,
	BM_PRUNE_TERNARY,
} BMCBTake;

typedef BMCBTake BMTraverseCB( BMTraverseData *, char **p, int flags, int f_next );
char *bm_traverse( char *expression, BMTraverseData *, int );

#define BMTraverseCBSwitch( func ) \
	static void func( void ) {
#define _prune( take ) \
		return take;
#define _return( val ) { \
		traverse_data->done = val; \
		return BM_DONE; }
#define _continue( q ) \
		return BM_DONE;
#define	_break \
		return BM_CONTINUE;
#define BMTraverseCBEnd \
	}


#endif	// TRAVERSE_H
