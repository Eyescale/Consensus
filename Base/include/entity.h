#ifndef ENTITY_H
#define ENTITY_H

#include "list.h"

typedef struct _Entity {
	struct _Entity **sub;
	listItem **as_sub;
} CNEntity;

CNEntity *cn_new( CNEntity *, CNEntity * );
CNEntity *cn_instance( CNEntity *, CNEntity *, const int pivot );
void cn_rewire( CNEntity *, int ndx, CNEntity *sub );
void cn_prune( CNEntity * );
void cn_release( CNEntity * );
void cn_free( CNEntity * );

#define CNSUB(e,ndx) \
	( (e)->sub[!(ndx)] ? (e)->sub[ndx] : NULL )
#define isBase( e ) \
	( !(e)->sub[0] || !(e)->sub[1] )


#endif	// ENTITY_H
