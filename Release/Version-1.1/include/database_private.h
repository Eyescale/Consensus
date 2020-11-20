#ifndef DB_PRIVATE_H
#define	DB_PRIVATE_H

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

//===========================================================================
//	core
//===========================================================================
static CNInstance *cn_new( CNInstance *, CNInstance * );
static void cn_free( CNInstance * );
static CNInstance *cn_instance( CNInstance *, CNInstance * );


#endif	// DB_PRIVATE_H
