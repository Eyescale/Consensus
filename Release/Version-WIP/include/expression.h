#ifndef EXPRESSION_H
#define EXPRESSION_H

#include "context.h"
#include "query.h"

CNInstance *	bm_feel( int, char *, BMContext * );
listItem *	bm_scan( char *, BMContext * );
int		bm_tag_traverse( char *, char *, BMContext * );
int		bm_tag_inform( char *, char *, BMContext * );
int		bm_tag_clear( char *, BMContext * );
void		bm_release( char *, BMContext * );
int		bm_inputf( char *fmt, listItem *args, BMContext * );
int		bm_outputf( FILE *, char *fmt, listItem *args, BMContext * );
void		fprint_expr( FILE *, char *, int level );

#define TAB( level ) \
	for ( int k=0; k<level; k++ ) fprintf( stream, "\t" );

//---------------------------------------------------------------------------
//	bufferized output
//---------------------------------------------------------------------------
typedef struct {
	FILE *stream;
	int type, first;
	CNInstance *last;
} OutputData;

void	bm_out_put( OutputData *, CNInstance *, CNDB * );
int	bm_out_flush( OutputData *, CNDB * );


#endif	// EXPRESSION_H
