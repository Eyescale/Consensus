#ifndef DATABASE_H
#define DATABASE_H

#include "entity.h"
#include "registry.h"
#include "string_util.h"

//===========================================================================
//	newCNDB / freeCNDB
//===========================================================================
typedef CNEntity CNInstance;
typedef struct {
	CNInstance *nil;
	Registry *index;
} CNDB;

CNDB *	newCNDB( CNEntity * );
void	freeCNDB( CNDB * );

//===========================================================================
//	data
//===========================================================================
typedef int DBTraverseCB( CNInstance *, CNDB *, void * );

CNInstance *	db_share( uint master[2], Registry *ref, CNDB * );
CNInstance *	new_proxy( CNEntity *, CNEntity *, CNDB * );
void		free_proxy( CNEntity *, CNDB * );
CNInstance *	db_register( char *identifier, CNDB * );
void		db_deregister( CNInstance *, CNDB * );
void		db_deprecate( CNInstance *, CNDB * );
void		db_signal( CNInstance *, CNDB * );
int		db_match( CNInstance *, CNDB *, CNInstance *, CNDB * );
int		db_match_identifier( CNInstance *, char * );
CNInstance *	db_instantiate( CNInstance *, CNInstance *, CNDB * );
CNInstance *	db_assign( CNInstance *, CNInstance *, CNDB * );
CNInstance *	db_unassign( CNInstance *, CNDB * );
CNInstance *	db_lookup( int privy, char *identifier, CNDB * );
listItem *	db_rxscan( int privy, char *regex, CNDB * );
int		db_traverse( int privy, CNDB *, DBTraverseCB, void * );
CNInstance *	DBFirst( CNDB *, listItem ** );
CNInstance *	DBNext( CNDB *, CNInstance *, listItem ** );
void		db_fire( CNInstance *, CNDB * );

static inline void
db_clear( CNInstance *e, int ndx, CNDB *db ) {
	for ( listItem *i=e->as_sub[ndx]; i!=NULL; i=i->next )
		db_deprecate( i->ptr, db ); }

//---------------------------------------------------------------------------
//	i/o
//---------------------------------------------------------------------------
int	db_outputf( CNDB *, FILE *, char *fmt, ... );

//---------------------------------------------------------------------------
//	sub
//---------------------------------------------------------------------------
#define CNSUB(e,ndx) (( e==e->sub[1] || !e->sub[!(ndx)] ) ? NULL : e->sub[ndx] )

#define isBase(e) (!CNSUB(e,0))

static inline CNEntity * xpn_sub( CNEntity *x, listItem *xpn ) {
	if ( !xpn ) return x;
	for ( listItem *i=xpn; i!=NULL; i=i->next ) {
		x = CNSUB( x, cast_i(i->ptr) );
		if ( !x ) return NULL; }
	return x; }

static inline int cnIsIdentifier( CNInstance *e ) {
	return ( e->sub[1]==e ); }
static inline char * CNIdentifier( CNInstance *e ) {
	return (char *) e->sub[ 0 ]; }
static inline int cnStarMatch( CNInstance *e ) {
	return (( e ) && cnIsIdentifier(e) && *CNIdentifier(e)=='*' ); }

static inline int cnIsShared( CNInstance *e ) {
	return ( !e->sub[ 0 ] ); }
static inline uint * CNSharedKey( CNInstance *e ) {
	return (uint *) &e->sub[1]; }
static inline int cnIsUnnamed( CNInstance *e ) {
	return CNSharedKey(e)[0]=='!'; }

static inline int cnIsProxy( CNInstance *e ) {
	return ((e->sub[0]) && !e->sub[1]); }
static inline CNEntity * CNProxyThis( CNInstance *proxy ) {
	return (CNEntity*) proxy->sub[0]->sub[0]; }
static inline CNEntity * CNProxyThat( CNInstance *proxy ) {
	return (CNEntity*) proxy->sub[0]->sub[1]; }
static inline int cnIsSelf( CNInstance *proxy ) {
	return !CNProxyThis(proxy); }

static inline CNEntity * DBCell( CNDB *db ) {
	return CNProxyThat( db->nil->sub[0] ); }

//===========================================================================
//	operation (nil-based)
//===========================================================================
#include "db_op.h"

//---------------------------------------------------------------------------
//	db_private, db_deprecated, db_manifested
//---------------------------------------------------------------------------
static inline int db_private( int privy, CNInstance *e, CNDB *db )
/*
	called by xp_traverse() and xp_verify()
	. if privy==0
		returns 1 if either newborn (not reassigned) or released
		returns 0 otherwise
	. if privy==1 - traversing db_log( released, ... )
		returns 1 if newborn (not reassigned)
		returns 0 otherwise
	. if privy==2 - traversing %| or in eeno_query_assignment()
		returns 1 if e:( ., nil ) or e:( nil, . )
		returns 0 otherwise
*/ {
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

//---------------------------------------------------------------------------
//	db_init, db_exit, DBInitOn, DBExitOn
//---------------------------------------------------------------------------
#define SELF_PROXY(db) ((CNInstance *) db->nil->sub[0] )

static inline void db_init( CNDB *db ) {
	db_update( db ); }
static inline void db_exit( CNDB *db ) {
	db_op( DB_DEPRECATE_OP, SELF_PROXY(db), db ); }
static inline int DBInitOn( CNDB *db ) {
	return db_manifested( SELF_PROXY(db), db ); }
static inline int DBExitOn( CNDB *db ) {
	return db_deprecated( SELF_PROXY(db), db ); }

//---------------------------------------------------------------------------
//	DBActive
//---------------------------------------------------------------------------
static inline int DBActive( CNDB *db ) {
	listItem **as_sub = db->nil->as_sub;
	return ((as_sub[ 0 ]) || (as_sub[ 1 ]) || DBInitOn(db) || DBExitOn(db)); }


//===========================================================================
//	CNDB Shared Arena interface
//===========================================================================
#include "db_arena.h"


#endif	// DATABASE_H
