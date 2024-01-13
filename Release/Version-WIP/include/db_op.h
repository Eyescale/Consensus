#ifndef DB_OP_H
#define	DB_OP_H

#include "arena.h"

//===========================================================================
//	operations (nil-based)
//===========================================================================
typedef enum {
	DB_DEPRECATE_OP,
	DB_MANIFEST_OP,
	DB_REHABILITATE_OP,
	DB_REASSIGN_OP
} DBOperation;

int	db_op( DBOperation op, CNInstance *e, CNDB *db );

void	db_update( CNDB *, CNInstance *, Registry * );

CNInstance * DBLog( int first, int released, CNDB *, listItem ** );

#endif // DB_OP_H
