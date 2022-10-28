#ifndef PROXY_H
#define PROXY_H

#define bm_proxy_still( proxy )	db_still( BMThisDB( BMProxyThat(proxy) ) )
#define bm_proxy_in( proxy )	db_in( BMThisDB( BMProxyThat(proxy) ) )

void		bm_proxy_op( char *expression, BMContext * );
listItem *	bm_proxy_scan( char *, BMContext * );
CNInstance *	bm_proxy_feel( CNInstance *, BMQueryType, char *, BMContext * );


#endif	// PROXY_H
