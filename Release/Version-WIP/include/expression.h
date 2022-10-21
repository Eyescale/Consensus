#ifndef EXPRESSION_H
#define EXPRESSION_H

#include "narrative.h"
#include "context.h"
#include "query.h"

CNInstance *bm_feel( BMQueryType, char *expression, BMContext * );

char *	bm_deternarize( char **expression, BMContext * );
void	bm_instantiate( char *expression, BMContext *, CNStory * );
void	bm_release( char *expression, BMContext * );
void	bm_proxy_op( char *expression, BMContext * );
int	bm_outputf( char *format, listItem *args, BMContext * );
int	bm_inputf( char *format, listItem *args, BMContext * );


#endif	// EXPRESSION_H
