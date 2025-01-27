#ifndef DATABASE_H
#define DATABASE_H

#include "entity.h"
#include "registry.h"
#include "string_util.h"

typedef CNEntity CNInstance;
typedef struct {
	CNInstance *nil;
	Registry *index;
} CNDB;
typedef int DBTraverseCB( CNInstance *, CNDB *, void * );

//===========================================================================
//	newCNDB / freeCNDB
//===========================================================================
CNDB *	newCNDB( void );
void	freeCNDB( CNDB * );

//---------------------------------------------------------------------------
//	CNSUB
//---------------------------------------------------------------------------
#define CNSUB(e,ndx) ( e->sub[!(ndx)] ? e->sub[ndx] : NULL )

#define isBase( e ) (!CNSUB(e,0))

static inline int cn_hold( CNEntity *e, CNEntity *f ) {
	return ( CNSUB(e,0)==f || CNSUB(e,1)==f ); }

//===========================================================================
//	data
//===========================================================================
CNInstance *	db_register( char *identifier, CNDB * );
void		db_deprecate( CNInstance *, CNDB * );
void		db_clear( listItem *, CNDB * );
void		db_signal( CNInstance *, CNDB * );
int		db_match( CNInstance *, CNDB *, CNInstance *, CNDB * );
CNInstance *	db_instantiate( CNInstance *, CNInstance *, CNDB * );
CNInstance *	db_assign( CNInstance *, CNInstance *, CNDB * );
CNInstance *	db_unassign( CNInstance *, CNDB * );
CNInstance *	db_lookup( int privy, char *identifier, CNDB * );
int		db_traverse( int privy, CNDB *, DBTraverseCB, void * );
CNInstance *	DBFirst( CNDB *, listItem ** );
CNInstance *	DBNext( CNDB *, CNInstance *, listItem ** );

#define UNIFIED
#ifndef UNIFIED
	char *	DBIdentifier( CNInstance *, CNDB * );
	int	DBStarMatch( CNInstance *e, CNDB *db );
#else
static inline char * DBIdentifier( CNInstance *e, CNDB *db ) {
	return (char *) e->sub[ 1 ]; }
static inline int DBStarMatch( CNInstance *e, CNDB *db ) {
	return ((e) && !(e->sub[0]) && *(char*)e->sub[1]=='*' ); }
#endif

//===========================================================================
//	proxy
//===========================================================================
CNInstance *	db_proxy( CNEntity *, CNEntity *, CNDB * );
void		db_fire( CNInstance *, CNDB * );

//---------------------------------------------------------------------------
//	isProxy, DBProxyThis, DBProxyThat, isProxySelf
//---------------------------------------------------------------------------
static inline int isProxy( CNInstance *e ) {
	return ((e->sub[0]) && !e->sub[1]); }

static inline CNEntity * DBProxyThis( CNInstance *proxy ) {
	return (CNEntity*) proxy->sub[0]->sub[0]; }

inline CNEntity * DBProxyThat( CNInstance *proxy ) {
	return (CNEntity*) proxy->sub[0]->sub[1]; }

inline int isProxySelf( CNInstance *proxy ) {
	return !DBProxyThis(proxy); }

//---------------------------------------------------------------------------
//	i/o
//---------------------------------------------------------------------------
int	db_outputf( FILE *, CNDB *, char *fmt, ... );

//===========================================================================
//	op (nil-based)
//===========================================================================
#include "db_op.h"

//---------------------------------------------------------------------------
//	db_init, db_exit, DBInitOn, DBExitOn, DBActive
//---------------------------------------------------------------------------
static inline void db_init( CNDB *db ) {
	db->nil->sub[ 0 ] = db->nil; }

static inline void db_exit( CNDB *db ) {
	db->nil->sub[ 1 ] = db->nil; }

static inline int DBInitOn( CNDB *db ) {
	return !!db->nil->sub[ 0 ]; }

static inline int DBExitOn( CNDB *db ) {
	return !!db->nil->sub[ 1 ]; }

static inline int DBActive( CNDB *db ) {
	listItem **as_sub = db->nil->as_sub;
	return ((as_sub[ 0 ]) || (as_sub[ 1 ]) || DBInitOn(db) || DBExitOn(db)); }

//---------------------------------------------------------------------------
//	db_private, db_deprecated, db_manifested
//---------------------------------------------------------------------------
static inline int db_private( int privy, CNInstance *e, CNDB *db ) {
/*
	called by xp_traverse() and xp_verify()
	. if privy==0
		returns 1 if either newborn (not reassigned) or released
		returns 0 otherwise
	. if privy==1 - traversing db_log( released, ... )
		returns 1 if newborn (not reassigned)
		returns 0 otherwise
	. if privy==2 - traversing %| or in proxy_feel_assignment()
		returns 1 if e:( ., nil ) or e:( nil, . )
		returns 0 otherwise
*/
	CNInstance *nil = db->nil, *f;
	if ( cn_hold( e, nil ) )
		return 1;
	if ( privy==2 ) return 0;
	if (( f=cn_instance( e, nil, 1 ) )) {
		if ( f->as_sub[ 1 ] ) // to be released
			return !!f->as_sub[ 0 ]; // newborn
		if ( f->as_sub[ 0 ] ) // newborn or reassigned
			return !cn_instance( nil, e, 0 );
		return !privy; } // released
	return 0; }

static inline int db_deprecated( CNInstance *e, CNDB *db ) {
	CNInstance *nil = db->nil;
	CNInstance *f = cn_instance( e, nil, 1 );
	return ( f && !( f->as_sub[0] && !f->as_sub[1] )); }

static inline int db_manifested( CNInstance *e, CNDB *db ) {
	CNInstance *nil = db->nil, *f;
	if (( f=cn_instance( nil, e, 0 ) )) {
		return !f->as_sub[ 1 ]; }
	return 0; }


#endif	// DATABASE_H
