#ifndef DETERNARIZE_H
#define DETERNARIZE_H

#include "context.h"

char * bm_deternarize( char **expression, BMContext * );

void free_deternarized( listItem *sequence );



#endif	// DETERNARIZE_H
