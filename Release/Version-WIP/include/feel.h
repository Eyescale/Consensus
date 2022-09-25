#ifndef FEEL_H
#define FEEL_H

#include "context.h"

//===========================================================================
//	expression traversal
//===========================================================================
typedef enum {
        BM_CONDITION = 0,
        BM_RELEASED,
        BM_MANIFESTED
} BMLogType;
CNInstance *bm_feel( char *expression, BMContext *, BMLogType );

enum {
	BM_CONTINUE,
	BM_DONE
};
typedef int BMTraverseCB( CNInstance *, BMContext *, void * );
CNInstance *bm_traverse( char *expression, BMContext *, BMTraverseCB, void * );


#endif	// FEEL_H
