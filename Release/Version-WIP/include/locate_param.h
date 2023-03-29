#ifndef LOCATE_PARAM_H
#define LOCATE_PARAM_H

#include "list.h"

typedef void BMLocateCB( char *, listItem *, void * );
void bm_locate_param( char *, listItem **xpn, BMLocateCB, void * );


#endif	// LOCATE_PARAM_H
