#ifndef EXPRESSION_H
#define EXPRESSION_H

#include "context.h"
#include "query.h"

#define bm_feel( type, expression, ctx ) \
		bm_query( type, expression, ctx, NULL, NULL )
listItem *	bm_scan( char *, BMContext * );
void		bm_release( char *expression, BMContext * );
int		bm_void( char *, BMContext * );
int		bm_inputf( char *format, listItem *args, BMContext * );
int		bm_outputf( char *format, listItem *args, BMContext * );

char *		bm_deternarize( char **expression, BMContext * );

#endif	// EXPRESSION_H
