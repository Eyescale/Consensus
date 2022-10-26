#ifndef EXPRESSION_H
#define EXPRESSION_H

#include "narrative.h"
#include "context.h"
#include "query.h"

CNInstance *	bm_feel( BMQueryType, char *, BMContext * );
CNInstance *	bm_proxy_feel( CNInstance *, BMQueryType, char *, BMContext * );
void		bm_proxy_op( char *expression, BMContext * );
listItem *	bm_scan( char *, BMContext * );
listItem *	bm_scan_active( char *, BMContext * );
int		bm_void( char *, BMContext * );
void		bm_release( char *expression, BMContext * );
int		bm_outputf( char *format, listItem *args, BMContext * );
int		bm_inputf( char *format, listItem *args, BMContext * );

#define bm_proxy_still( proxy )	db_still( BMProxyDB(proxy) )
#define bm_proxy_in( proxy )	db_in( BMProxyDB(proxy) )

char *	bm_deternarize( char **expression, BMContext * );
void	bm_instantiate( char *expression, BMContext *, CNStory * );

#endif	// EXPRESSION_H
