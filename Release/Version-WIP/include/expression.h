#ifndef EXPRESSION_H
#define EXPRESSION_H

#include "context.h"

char *	bm_deternarize( char **expression, BMContext * );
void	bm_instantiate( char *expression, BMContext * );
int	bm_void( char *expression, BMContext * );
listItem * bm_query( char *expression, BMContext * );
void	bm_release( char *expression, BMContext * );
int	bm_assign_op( int op, char *expression, BMContext *, int * );
int	bm_outputf( char *format, listItem *args, BMContext * );
int	bm_inputf( char *format, listItem *args, BMContext * );


#endif	// EXPRESSION_H
