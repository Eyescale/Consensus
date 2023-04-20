#ifndef PROXY_H
#define PROXY_H

#include "context.h"
#include "query.h"

CNInstance *	bm_proxy_feel( CNInstance *, BMQueryType, char *, BMContext * );
listItem *	bm_proxy_scan( BMQueryType, char *, BMContext * );
int 		bm_proxy_active( CNInstance * );
int		bm_proxy_in( CNInstance * );
int		bm_proxy_out( CNInstance * );


#endif	// PROXY_H
