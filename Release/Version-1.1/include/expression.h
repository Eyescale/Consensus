#ifndef EXPRESSION_H
#define EXPRESSION_H

#include "context.h"
#include "database.h"
#include "traversal.h"

void	bm_substantiate( char *expression, BMContext * );
listItem * bm_query( char *expression, BMContext * );
void	bm_release( char *expression, BMContext * );
int	bm_outputf( char *format, listItem *args, BMContext * );
int	bm_inputf( char *format, listItem *args, BMContext * );



#endif	// EXPRESSION_H
