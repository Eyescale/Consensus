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

#define EMARK		1
#define QMARK		2
#define PMARK		4
#define THIS		8
#define IDENTIFIER	16
#define CHARACTER	32
#define	MOD		64
#define	STAR		128

int bm_scour( char *expression, int target );


#endif	// LOCATE_H
