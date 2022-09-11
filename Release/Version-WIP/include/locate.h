#ifndef LOCATE_H
#define LOCATE_H

#include "list.h"

typedef void BMLocateCB( char *, listItem *, void * );

char *	bm_locate_pivot( char *, listItem ** );
char *	bm_locate_mark( char *, listItem ** );
char *	bm_locate_param( char *, listItem **xpn, BMLocateCB, void * );

#define AS_SUB  0
#define SUB     2

void xpn_add( listItem **xp, int as_sub, int position );
void xpn_set( listItem *xp, int as_sub, int position );
void xpn_out( FILE *stream, listItem *xp );

//===========================================================================
//      target acquisition (internal)
//===========================================================================

#define QMARK		1
#define IDENTIFIER	2
#define CHARACTER	4
#define	MOD		8
#define	STAR		16
#define EMARK		32

int xp_target( char *expression, int target );


#endif	// LOCATE_H
