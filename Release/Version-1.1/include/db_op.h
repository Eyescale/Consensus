#ifndef DB_OP_H
#define	DB_OP_H

#include "as_sub.h"

//===========================================================================
//	core
//===========================================================================
CNInstance *cn_new( CNInstance *, CNInstance * );
void cn_free( CNInstance * );

//===========================================================================
//	operations (nil-based)
//===========================================================================
typedef enum {
	DB_EXIT_OP,
	DB_DEPRECATE_OP,
	DB_MANIFEST_OP,
	DB_REHABILITATE_OP,
	DB_REASSIGN_OP
} DBOperation;
int db_op( DBOperation op, CNInstance *e, CNDB *db );
int db_deprecatable( CNInstance *, CNDB * );

void db_remove( CNInstance *, CNDB * );
void db_deregister( CNInstance *, CNDB * );

#define cn_instance( e, f, p ) \
	lookup_as_sub( e, f, p )


#endif // DB_OP_H
