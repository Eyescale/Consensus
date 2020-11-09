#ifndef EXPRESSION_H
#define EXPRESSION_H

#include "context.h"
#include "database.h"
#include "traversal.h"

int     bm_verify( CNInstance **, char **, BMTraverseData * );
void	bm_substantiate( char *expression, BMContext * );
listItem * bm_query( char *expression, BMContext * );
void	bm_release( char *expression, BMContext * );
void	bm_outputf( char *format, char *expression, BMContext * );
void	bm_output( char *expression, BMContext * );


#endif	// EXPRESSION_H
