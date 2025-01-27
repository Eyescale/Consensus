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

CNDB *	newCNDB( void );
void	freeCNDB( CNDB * );

//===========================================================================
//	data
//===========================================================================
typedef int DBTraverseCB( CNInstance *, CNDB *, void * );

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
CNInstance *	new_proxy( CNEntity *, CNEntity *, CNDB * );
void		free_proxy( CNEntity *e );
void		db_fire( CNInstance *, CNDB * );

//---------------------------------------------------------------------------
//	i/o
//---------------------------------------------------------------------------
int	db_outputf( CNDB *, FILE *, char *fmt, ... );

//---------------------------------------------------------------------------
//	sub
//---------------------------------------------------------------------------
#define CNSUB(e,ndx) (( e==e->sub[1] || !e->sub[!(ndx)] ) ? NULL : e->sub[ndx] )

#define isBase(e) (!CNSUB(e,0))

static inline int cn_hold( CNEntity *e, CNEntity *f ) {
	return ( CNSUB(e,0)==f || CNSUB(e,1)==f ); }

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
static inline char * CNSharedIdentifier( CNInstance *e ) {
	return (char *) ((Pair *)e->sub[1])->name; }
static inline int cnIsUnnamed( CNInstance *e ) {
	return !CNSharedIdentifier(e); }

//---------------------------------------------------------------------------
//	proxy
//---------------------------------------------------------------------------
static inline CNEntity * CNProxyThis( CNInstance *proxy ) {
	return (CNEntity*) proxy->sub[0]->sub[0]; }
static inline CNEntity * CNProxyThat( CNInstance *proxy ) {
	return (CNEntity*) proxy->sub[0]->sub[1]; }

static inline int cnIsProxy( CNInstance *e ) {
	return ((e->sub[0]) && !e->sub[1]); }
static inline int cnIsSelf( CNInstance *proxy ) {
	return !CNProxyThis(proxy); }

//===========================================================================
//	operation (nil-based)
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
	. if privy==2 - traversing %| or in eeno_query_assignment()
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
static inline int db_has_newborn( listItem *list, int sub, CNDB *db )
/*
   argument
	sub==1 => list=list.sub[0]
	sub==2 => list=list.sub[1]
   returns
	1 if list contains newborn or reassigned
	0 otherwise
*/ {
	CNInstance *nil = db->nil, *e, *f, *g;
	listItem *i, *j, *k;
	for ( i=list; i!=NULL; i=i->next ) {
		e = i->ptr;
		if ( !e ) continue; // not supported
		else if ( sub==1 ) e = CNSUB( e, 0 );
		else if ( sub==2 ) e = CNSUB( e, 1 );
		if ( !e ) continue;
		for ( j=e->as_sub[0]; j!=NULL; j=j->next ) {
			f = j->ptr;
			if ( f->sub[1]!=nil ) continue;
			for ( k=f->as_sub[0]; k!=NULL; k=k->next ) {
				g = k->ptr;
				if ( g->sub[1]==nil )
					return 1; } } }
	return 0; }


#endif	// DATABASE_H
