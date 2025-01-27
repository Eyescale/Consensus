#ifndef LOCATE_PARAM_H
#define LOCATE_PARAM_H

#include "entity.h"

typedef void BMLocateCB( char *, listItem *, void * );
char *	bm_locate_param( char *, listItem **xpn, BMLocateCB, void * );
#define bm_locate_mark( expr, xpn ) \
	bm_locate_param( expr, xpn, NULL, NULL )


#endif	// LOCATE_PARAM_H
