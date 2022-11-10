#ifndef PROXY_H
#define PROXY_H

#include "context.h"
#include "query.h"

listItem *	bm_proxy_scan( BMQueryType, char *, BMContext * );
int 		bm_proxy_still( CNInstance * );
int		bm_proxy_in( CNInstance * );
int		bm_proxy_out( CNInstance * );
CNInstance *	bm_proxy_feel( CNInstance *, BMQueryType, char *, BMContext * );


#endif	// PROXY_H
