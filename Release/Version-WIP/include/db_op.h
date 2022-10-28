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
int db_deprecatable( CNInstance *, CNDB * );
int db_to_be_manifested( CNInstance *, CNDB * );

//---------------------------------------------------------------------------
//	db_star / db_still / db_init / db_exit / db_in / db_out
//---------------------------------------------------------------------------
/*
	Assumption: CNDB is freed upon exit, so that no attempt to access
	db->nil will be made after db_exit() - which overwrites nil->sub[1]
*/
inline CNInstance * db_star( CNDB *db ) {
	CNInstance *nil = db->nil;
	return nil->sub[ 1 ]; }
inline int db_still( CNDB *db ) {
	CNInstance *nil = db->nil;
	return !( nil->as_sub[0] || nil->as_sub[1] ); }
inline void db_init( CNDB *db ) {
	CNInstance *nil = db->nil;
	nil->sub[ 0 ] = nil; }
inline void db_exit( CNDB *db ) {
	CNInstance *nil = db->nil;
	nil->sub[ 1 ] = nil; }
inline int db_in( CNDB *db ) {
	CNInstance *nil = db->nil;
	return ( nil->sub[0]==nil ); }
inline int db_out( CNDB *db ) {
	CNInstance *nil = db->nil;
	return ( nil->sub[1]==nil ); }


#endif // DB_OP_H
