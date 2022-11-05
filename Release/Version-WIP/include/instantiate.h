#ifndef INSTANTIATE_H
#define	INSTANTIATE_H

#include "expression.h"
#include "program.h"

// #define DEBUG

#ifdef DEBUG
#define DBG_VOID( p ) \
	if ( bm_void( p, ctx ) ) { \
		fprintf( stderr, ">>>>> B%%: bm_instantiate(): VOID: %s\n", p ); \
		exit( -1 ); }
#else
#define	DBG_VOID( p )
#endif

void bm_instantiate( char *expression, BMContext *, CNStory * );


#endif	// INSTANTIATE_H
