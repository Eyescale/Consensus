#ifndef LOCATE_H
#define LOCATE_H

#include "list.h"

char *	bm_locate_pivot( char *, listItem ** );

#define AS_SUB  0
#define SUB     2

void xpn_add( listItem **xp, int as_sub, int position );
void xpn_set( listItem *xp, int as_sub, int position );
void xpn_out( FILE *stream, listItem *xp );


#endif	// LOCATE_H
