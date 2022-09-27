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

typedef int BMScanCB( CNInstance *, BMContext *, void * );
CNInstance *bm_scan( char *expression, BMContext *, BMScanCB, void * );


#endif	// FEEL_H
