#ifndef TRAVERSE_H
#define TRAVERSE_H

#include "flags.h"

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

typedef enum {
	BM_CONTINUE,
	BM_DONE
} BMCBTake;
typedef enum {
	BMBgnCB = 0,
	BMPreemptCB,
	BMNegatedCB,		// ~
	BMBgnSetCB,		// {
	BMEndSetCB,		// }
	BMBgnPipeCB,		// |
	BMModCharacterCB, 	// % followed by one of ,:)}|
	BMMarkRegisterCB,	// %? or %!
	BMSubExpressionCB,	// %(
	BMStarCharacterCB,	// * followed by one of ,:)}|
	BMDereferenceCB,	// *
	BMLiteralCB,		// (:
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
	BMSyntaxErrorCB,
	BMEndCB,
	BMCBNum
} BMCBName;
typedef struct {
	void *table[ BMCBNum ];
	void *user_data;
	int flags, f_next;
	int done;
	char *p;
} BMTraverseData;

typedef int BMTraverseCB( BMTraverseData *, char *p, int flags );
char *bm_traverse( char *expression, BMTraverseData * );

#define BMTraverseCBBegin
#define BMTraverseCBEnd


#endif	// TRAVERSE_H
