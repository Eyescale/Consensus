#ifndef DB_TYPE_H
#define	DB_TYPE_H

typedef enum {
	CN_NEWBORN = 0,
	CN_NEWBORN_TO_BE_RELEASED,
	CN_MANIFESTED,
	CN_MANIFESTED_TO_BE_RELEASED,
	CN_MANIFESTED_REASSIGNED,
	CN_RELEASED,
	CN_RELEASED_TO_BE_MANIFESTED,
	CN_TO_BE_MANIFESTED,
	CN_TO_BE_RELEASED,
	CN_DEFAULT, // keep before last
	CN_PRIVATE  // keep last
} CNType;
CNType cn_type( CNInstance *e, CNInstance *f, CNInstance *g, CNDB * );

int db_op_debug( DBOperation, CNInstance *, CNDB * );
void db_update_debug( CNDB * );

CNType test_instantiate( char *src, CNInstance *, CNDB * );
CNType test_deprecate( char *src, CNInstance *, CNDB * );


#endif	// DB_TYPE_H
