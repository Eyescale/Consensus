#ifndef UTIL_H
#define UTIL_H

#include "database.h"
#include "string_util.h"

#define AS_SUB	0
#define SUB	2

void xpn_add( listItem **xp, int as_sub, int position );
void xpn_out( FILE *stream, listItem *xp );

typedef void BMLocateCB( char *, listItem *, void * );

char *	p_locate_param( char *expression, listItem **exponent, BMLocateCB, void * );
char *	p_locate_mark( char *expression, listItem **exponent );
char *	p_locate( char *expression, listItem **exponent );

void dbg_out( char *pre, CNInstance *e, char *post, CNDB *db );


#endif	// UTIL_H
