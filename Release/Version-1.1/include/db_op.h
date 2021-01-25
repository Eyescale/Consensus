#ifndef DB_OP_H
#define	DB_OP_H

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


#endif // DB_OP_H
