#ifndef EENO_H
#define EENO_H

#include "context.h"
#include "query.h"

void *		bm_eeno_feel( CNInstance *, int, char *, BMContext * );
listItem *	bm_proxy_scan( int, char *, BMContext * );
int 		bm_proxy_active( CNInstance * );
int		bm_proxy_in( CNInstance * );
int		bm_proxy_out( CNInstance * );


#endif	// EENO_H
