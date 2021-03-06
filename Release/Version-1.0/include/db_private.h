#ifndef DB_PRIVATE_H
#define	DB_PRIVATE_H

static CNInstance *db_instantiate( CNInstance *, CNInstance *, CNDB * );
static char *db_identifier( CNInstance *, CNDB * );
static void db_deregister( CNInstance *, CNDB * );
static void db_remove( CNInstance *, CNDB * );
static int db_deprecated( CNInstance *, CNDB * );

//===========================================================================
//	nil-based operations
//===========================================================================
enum {
	DB_DEPRECATE_OP,
	DB_MANIFEST_OP,
	DB_REHABILITATE_OP,
	DB_REASSIGN,
	DB_NEW
};
static CNInstance *db_op( int op, CNInstance *e, int flags, CNDB *db );


#endif	// DB_PRIVATE_H
