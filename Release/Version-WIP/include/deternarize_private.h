#ifndef DETERNARIZE_PRIVATE_H
#define DETERNARIZE_PRIVATE_H

#include "string_util.h"
#include "traverse.h"

static void free_deternarized( listItem *sequence );
static void s_scan( CNString *, listItem * );

typedef int BMTernaryCB( char *, void * );


#endif	// DETERNARIZE_PRIVATE_H
