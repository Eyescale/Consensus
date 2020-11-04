#ifndef UTIL_H
#define UTIL_H

#include "database.h"
#include "list.h"

#define AS_SUB	0
#define SUB	2

void xpn_add( listItem **xp, int as_sub, int position );
void xpn_out( FILE *stream, listItem *xp );

typedef enum {
	PRUNE_DEFAULT,
	PRUNE_IDENTIFIER,
	PRUNE_COLON
} PruneType;
char *	p_locate( char *expression, char *fmt, listItem **exponent );
char *	p_extract( char * );
char *	p_prune( PruneType type, char * );
int	p_single( char * );
int	p_filtered( char * );

void dbg_out( char *pre, CNInstance *e, char *post, CNDB *db );


#endif	// UTIL_H
