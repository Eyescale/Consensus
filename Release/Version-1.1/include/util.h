#ifndef UTIL_H
#define UTIL_H

#include "database.h"

#define AS_SUB	0
#define SUB	2

void xpn_add( listItem **xp, int as_sub, int position );
void xpn_out( FILE *stream, listItem *xp );

typedef enum {
	PRUNE_DEFAULT,
	PRUNE_IDENTIFIER,
	PRUNE_COLON
} PruneType;
typedef void BMLocateCB( char *, listItem *, void * );

char *	p_locate( char *expression, listItem **exponent );
char *	p_locate_arg( char *expression, listItem **exponent, BMLocateCB, void * );
char *	p_extract( char * );
char *	p_prune( PruneType type, char * );
int	p_single( char * );
int	p_filtered( char * );

void dbg_out( char *pre, CNInstance *e, char *post, CNDB *db );


#endif	// UTIL_H
