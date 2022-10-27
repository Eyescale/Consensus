#ifndef EXPRESSION_H
#define EXPRESSION_H

#include "narrative.h"
#include "context.h"
#include "query.h"

#define bm_feel( type, expression, ctx ) \
		bm_query( type, expression, ctx, NULL, NULL )
	
listItem *	bm_scan( char *, BMContext * );
void		bm_release( char *expression, BMContext * );
int		bm_void( char *, BMContext * );

#define bm_proxy_still( proxy )	db_still( BMThisDB( BMProxyThat(proxy) ) )
#define bm_proxy_in( proxy )	db_in( BMThisDB( BMProxyThat(proxy) ) )
#define bm_proxy_feel( proxy, type, expression, ctx ) \
		bm_query( type, expression, ctx, NULL, BMProxyThat(proxy) );

void		bm_proxy_op( char *expression, BMContext * );
listItem *	bm_proxy_scan( char *, BMContext * );

int		bm_inputf( char *format, listItem *args, BMContext * );
int		bm_outputf( char *format, listItem *args, BMContext * );

char *		bm_deternarize( char **expression, BMContext * );
void		bm_instantiate( char *expression, BMContext *, CNStory * );

#endif	// EXPRESSION_H
