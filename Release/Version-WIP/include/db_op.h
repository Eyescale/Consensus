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

//---------------------------------------------------------------------------
//	db_star / db_still / db_init / db_exit / db_in / db_out
//---------------------------------------------------------------------------
/*
	Assumption: CNDB is freed upon exit, so that no attempt to access
	db->nil will be made after db_exit() - which overwrites nil->sub[ 1 ]
*/
#define	db_init( db ) \
	{ db->nil->sub[ 0 ] = db->nil; }
#define	db_exit( db ) \
	{ db->nil->sub[ 1 ] = db->nil; }
#define db_signal( x, db ) \
	{ db_op( DB_SIGNAL_OP, x, db ); }
#define db_manifest( x, db ) \
	{ db_op( DB_MANIFEST_OP, x, db ); }

#define db_star( db )	( db->nil->sub[ 1 ] )
#define	db_still( db )	( !db->nil->as_sub[ 0 ] && !db->nil->as_sub[ 1 ] )
#define	db_in( db )	( db->nil->sub[ 0 ]==db->nil )
#define	db_out( db )	( db->nil->sub[ 1 ]==db->nil )



#endif // DB_OP_H
