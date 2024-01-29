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

//===========================================================================
//	data
//===========================================================================
CNInstance *	db_register( char *identifier, CNDB * );
void		db_deregister( CNInstance *, CNDB * );
void		db_deprecate( CNInstance *, CNDB * );
void		db_clear( listItem *, CNDB * );
void		db_signal( CNInstance *, CNDB * );
int		db_match( CNInstance *, CNDB *, CNInstance *, CNDB * );
int		db_match_identifier( CNInstance *, char * );
CNInstance *	db_instantiate( CNInstance *, CNInstance *, CNDB * );
CNInstance *	db_assign( CNInstance *, CNInstance *, CNDB * );
CNInstance *	db_unassign( CNInstance *, CNDB * );
CNInstance *	db_lookup( int privy, char *identifier, CNDB * );
int		db_traverse( int privy, CNDB *, DBTraverseCB, void * );
CNInstance *	DBFirst( CNDB *, listItem ** );
CNInstance *	DBNext( CNDB *, CNInstance *, listItem ** );

static inline int isRef( CNInstance *e ) {
	return ( ((Pair*) e->sub[ 1 ])->value!=e ); }
static inline char * DBIdentifier( CNInstance *e ) {
	return (char *) ((Pair*) e->sub[ 1 ])->name; }
static inline int DBStarMatch( CNInstance *e ) {
	return ((e) && !(e->sub[0]) && *DBIdentifier(e)=='*' ); }

//===========================================================================
//	proxy
//===========================================================================
CNInstance *	new_proxy( CNEntity *, CNEntity *, CNDB * );
void		free_proxy( CNEntity *e );
void		db_fire( CNInstance *, CNDB * );

//---------------------------------------------------------------------------
//	isProxy, DBProxyThis, DBProxyThat, isProxySelf
//---------------------------------------------------------------------------
static inline int isProxy( CNInstance *e ) {
	return ((e->sub[0]) && !e->sub[1]); }

static inline CNEntity * DBProxyThis( CNInstance *proxy ) {
	return (CNEntity*) proxy->sub[0]->sub[0]; }

static inline CNEntity * DBProxyThat( CNInstance *proxy ) {
	return (CNEntity*) proxy->sub[0]->sub[1]; }

static inline int isProxySelf( CNInstance *proxy ) {
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
	db_update( db, NULL, NULL );
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

//---------------------------------------------------------------------------
//	db_has_newborn
//---------------------------------------------------------------------------
static inline int db_has_newborn( listItem *list, CNDB *db )
/*
   returns
	1 list contains newborn or reassigned
	0 otherwise
*/ {
	CNInstance *nil = db->nil, *e, *f, *g;
	listItem *i, *j, *k;
	for ( i=list; i!=NULL; i=i->next ) {
		e = i->ptr;
		if ( !e ) return !!DBLog( 1, 0, db, NULL );
		for ( j=e->as_sub[0]; j!=NULL; j=j->next ) {
			f = j->ptr;
			if ( f->sub[1]!=nil ) continue;
			for ( k=f->as_sub[0]; k!=NULL; k=k->next ) {
				g = k->ptr;
				if ( g->sub[1]==nil )
					return 1; } } }
	return 0; }


#endif	// DATABASE_H
