#ifndef DB_OP_H
#define	DB_OP_H

//===========================================================================
//	operations (nil-based)
//===========================================================================
typedef enum {
	DB_DEPRECATE_OP,
	DB_MANIFEST_OP,
	DB_MANIFEST_LOCALE_OP,
	DB_REHABILITATE_OP,
	DB_REASSIGN_OP
	} DBOperation;

typedef enum {
	DB_NEWBORN = 0,
	DB_NEWBORN_TO_BE_RELEASED,
	DB_MANIFESTED,
	DB_MANIFESTED_TO_BE_RELEASED,
	DB_MANIFESTED_TO_BE_MANIFESTED,
	DB_RELEASED,
	DB_RELEASED_TO_BE_RELEASED,
	DB_RELEASED_TO_BE_MANIFESTED,
	DB_TO_BE_MANIFESTED,
	DB_TO_BE_RELEASED,
	DB_DEFAULT, // keep before last
	DB_PRIVATE  // keep last
	} DBType;

void		db_update( CNDB * );
int		db_op( DBOperation op, CNInstance *e, CNDB *db );
DBType		db_type( CNInstance *, CNDB * );
CNInstance *	DBLog( int first, int released, CNDB *, listItem ** );


#endif // DB_OP_H
