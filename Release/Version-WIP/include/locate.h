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
#define	PARENT		8
#define THIS		16
#define IDENTIFIER	32
#define CHARACTER	64
#define	MOD		128
#define	STAR		256

int bm_scour( char *expression, int target );


#endif	// LOCATE_H
