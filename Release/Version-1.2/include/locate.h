#ifndef LOCATE_H
#define LOCATE_H

#include "list.h"

typedef void BMLocateCB( char *, listItem *, void * );

char *	bm_locate_pivot( char *, listItem ** );
char *	bm_locate_mark( char *, listItem ** );
char *	bm_locate_param( char *, listItem **xpn, BMLocateCB, void * );

#define AS_SUB  0
#define SUB     2

//===========================================================================
//      target acquisition (internal)
//===========================================================================

#define THIS		1
#define QMARK		2
#define IDENTIFIER	4
#define CHARACTER	8
#define	MOD		16
#define	STAR		32
#define PMARK		64

int bm_scour( char *expression, int target );


#endif	// LOCATE_H
