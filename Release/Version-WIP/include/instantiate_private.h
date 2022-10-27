#ifndef INSTANTIATE_PRIVATE_H
#define	INSTANTIATE_PRIVATE_H

#include "instantiate.h"
#include "locate.h"	// needed for one call to bm_scour() in bm_void()

#define case_( func ) \
	} static BMCBTake func( BMTraverseData *traverse_data, char **q, int flags, int f_next ) { \
		BMInstantiateData *data = traverse_data->user_data; char *p = *q;
#define current \
	( is_f( FIRST ) ? 0 : 1 )

static listItem *bm_couple( listItem *sub[2], CNDB * );
static CNInstance *bm_literal( char **, CNDB * );
static listItem *bm_list( char **, listItem **, CNDB * );


#endif	// INSTANTIATE_PRIVATE_H
