#ifndef DB_DEBUG_H
#define	DB_DEBUG_H

typedef enum {
	DB_NEWBORN = 0,
	DB_NEWBORN_TO_BE_RELEASED,
	DB_MANIFESTED,
	DB_MANIFESTED_TO_BE_RELEASED,
	DB_MANIFESTED_REASSIGNED,
	DB_RELEASED,
	DB_RELEASED_TO_BE_MANIFESTED,
	DB_TO_BE_MANIFESTED,
	DB_TO_BE_RELEASED,
	DB_DEFAULT, // keep before last
	DB_PRIVATE  // keep last
} DBGType;

int db_op_debug( DBOperation, CNInstance *, CNDB * );
void db_update_debug( CNDB * );

DBGType test_instantiate( char *src, CNInstance *, CNDB * );
DBGType test_deprecate( char *src, CNInstance *, CNDB * );


#endif	// DB_DEBUG_H
