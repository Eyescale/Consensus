#ifndef PROXY_H
#define PROXY_H

#include "context.h"
#include "query.h"

void		bm_proxy_op( char *expression, BMContext * );
listItem *	bm_proxy_scan( char *, BMContext * );
int 		bm_proxy_still( CNInstance * );
int		bm_proxy_in( CNInstance * );
CNInstance *	bm_proxy_feel( CNInstance *, BMQueryType, char *, BMContext * );
int		eeno_match( BMContext *, char *, CNDB *, CNInstance * );
CNInstance *	eeno_inform( BMContext *, CNDB *, char *, BMContext * );
CNInstance *	eeno_lookup( BMContext *, CNDB *, char * );


#endif	// PROXY_H
