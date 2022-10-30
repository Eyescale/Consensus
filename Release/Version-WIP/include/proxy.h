#ifndef PROXY_H
#define PROXY_H

void		bm_proxy_op( char *expression, BMContext * );
listItem *	bm_proxy_scan( char *, BMContext * );
CNInstance *	bm_proxy_feel( CNInstance *, BMQueryType, char *, BMContext * );


#endif	// PROXY_H
