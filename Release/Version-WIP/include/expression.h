#ifndef EXPRESSION_H
#define EXPRESSION_H

#include "context.h"
#include "query.h"

CNInstance *	bm_feel( int, char *, BMContext * );
listItem *	bm_scan( char *, BMContext * );
int		bm_tag_register( char *, char *, BMContext * );
int		bm_tag_traverse( char *, char *, BMContext * );
int		bm_tag_inform( char *, char *, BMContext * );
int		bm_tag_clear( char *, BMContext * );
void		bm_release( char *, BMContext * );
int		bm_inputf( char *fmt, listItem *args, BMContext * );
int		bm_outputf( FILE *, char *fmt, listItem *args, BMContext * );

static inline Pair *
bm_switch( int type, char *expression, BMContext *ctx ) {
	CNInstance *e, *f;
	e = bm_query_assignee( type, expression, ctx );
	if ( !e ) return NULL;
	if ( cnStarMatch( f=e->sub[0] ) )
		return newPair(e,NULL); // e:(*,.)
	return newPair( f->sub[1], e->sub[1] ); }

static inline int
bm_case( char *expression, BMContext *ctx ) {
	return !strcmp( expression, "." ) ?  1 :
		!strcmp( expression, "~." ) ? !BMContextRVV( ctx, "*^^" ) :
		!!bm_query_assignee( 0, expression, ctx ); }

//---------------------------------------------------------------------------
//	bufferized output
//---------------------------------------------------------------------------
typedef struct {
	FILE *stream;
	char *fmt;
	int first;
	CNInstance *last;
} OutputData;

void	bm_out_put( OutputData *, CNInstance *, CNDB * );
int	bm_out_last( OutputData *, CNDB * );


#endif	// EXPRESSION_H
