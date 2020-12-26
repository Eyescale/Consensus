#ifndef DB_OP_H
#define	DB_OP_H

#include "macros.h"

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
void db_op( DBOperation op, CNInstance *e, CNDB *db );


#endif	// DB_OP_H
