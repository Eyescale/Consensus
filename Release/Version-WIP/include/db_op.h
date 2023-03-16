#ifndef DB_OP_H
#define	DB_OP_H

//===========================================================================
//	operations (nil-based)
//===========================================================================
typedef enum {
	DB_SIGNAL_OP,
	DB_DEPRECATE_OP,
	DB_MANIFEST_OP,
	DB_REHABILITATE_OP,
	DB_REASSIGN_OP
} DBOperation;

int db_op( DBOperation op, CNInstance *e, CNDB *db );
void db_update( CNDB *, CNInstance * );
CNInstance *db_log( int first, int released, CNDB *, listItem ** );
int db_private( int privy, CNInstance *, CNDB * );
int db_to_be_manifested( CNInstance *, CNDB * );
int db_deprecatable( CNInstance *, CNDB * );
int db_deprecated( CNInstance *, CNDB * );
int db_manifested( CNInstance *, CNDB * );

inline void db_init( CNDB *db )
	{ db->nil->sub[ 0 ] = db->nil; }
inline void db_exit( CNDB *db )
	{ db->nil->sub[ 1 ] = db->nil; }
inline int DBInitOn( CNDB *db )
	{ return !!db->nil->sub[ 0 ]; }
inline int DBExitOn( CNDB *db )
	{ return !!db->nil->sub[ 1 ]; }
inline void db_signal( CNInstance *x, CNDB *db )
	{ db_op( DB_SIGNAL_OP, x, db ); }
inline void db_manifest( CNInstance *x, CNDB *db )
	{ db_op( DB_MANIFEST_OP, x, db ); }

inline int DBActive( CNDB *db ) {
	listItem **as_sub = db->nil->as_sub;
	return ((as_sub[ 0 ]) || (as_sub[ 1 ]) || DBInitOn(db) || DBExitOn(db)); }


#endif // DB_OP_H
