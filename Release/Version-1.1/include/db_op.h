#ifndef DB_OP_H
#define	DB_OP_H

//===========================================================================
//	core
//===========================================================================
CNInstance *cn_new( CNInstance *, CNInstance * );
void cn_free( CNInstance * );
CNInstance *cn_instance( CNInstance *, CNInstance *, int pivot );

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

//===========================================================================
//	debug
//===========================================================================
#include "db_debug.h"


#endif	// DB_OP_H
