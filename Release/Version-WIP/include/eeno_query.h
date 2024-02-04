#ifndef EENO_QUERY_H
#define EENO_QUERY_H

#include "context.h"
#include "query.h"

void *		bm_eeno_query( int, char *, CNInstance *, BMContext * );
listItem *	bm_eeno_scan( char *src, BMContext * );
int 		bm_proxy_active( CNInstance * );
int		bm_proxy_in( CNInstance * );
int		bm_proxy_out( CNInstance * );


#endif	// EENO_QUERY_H
