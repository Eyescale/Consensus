#ifndef TRAVERSAL_H
#define TRAVERSAL_H

#include "context.h"

//===========================================================================
//	expression traversal
//===========================================================================
typedef enum {
        BM_CONDITION = 0,
        BM_RELEASED,
        BM_MANIFESTED
} BMLogType;
int bm_feel( char *expression, BMContext *, BMLogType );

enum {
	BM_CONTINUE,
	BM_DONE
};
typedef int BMTraverseCB( CNInstance *, BMContext *, void * );
int bm_traverse( char *expression, BMContext *, BMTraverseCB, void * );

//===========================================================================
//	target acquisition (internal)
//===========================================================================
#define QMARK		1
#define IDENTIFIER	2
#define CHARACTER	4
#define	MOD		8
#define	STAR		16
#define EMARK		32

int xp_target( char *expression, int target );


#endif	// TRAVERSAL_H
