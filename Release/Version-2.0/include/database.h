#ifndef DATABASE_H
#define DATABASE_H

#include "entity.h"
#include "registry.h"

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
#define CNSUB(e,ndx) ( !e->sub[!ndx] ? NULL : e->sub[ndx] )

#define isBase( e ) (!CNSUB(e,0))

static inline CNEntity * xpn_sub( CNEntity *x, listItem *xpn ) {
	if ( !xpn ) return x;
	for ( listItem *i=xpn; i!=NULL; i=i->next ) {
		x = CNSUB( x, cast_i(i->ptr) );
		if ( !x ) return NULL; }
	return x; }

//===========================================================================
//	data
//===========================================================================
CNInstance *	db_register( char *identifier, CNDB * );
int		db_match( CNDB *, CNInstance *, CNDB *, CNInstance * );
CNInstance *	db_proxy( CNEntity *, CNEntity *, CNDB * );
CNInstance *	db_instantiate( CNInstance *, CNInstance *, CNDB * );
CNInstance *	db_assign( CNInstance *, CNInstance *, CNDB * );
CNInstance *	db_unassign( CNInstance *, CNDB * );
void		db_deprecate( CNInstance *, CNDB * );
CNInstance *	db_lookup( int privy, char *identifier, CNDB * );
int		db_traverse( int privy, CNDB *, DBTraverseCB, void * );
CNInstance *	DBFirst( CNDB *, listItem ** );
CNInstance *	DBNext( CNDB *, CNInstance *, listItem ** );

#define UNIFIED
#ifdef UNIFIED
inline char * DBIdentifier( CNInstance *e, CNDB *db )
	{ return (char *) e->sub[ 1 ]; }
inline int DBStarMatch( CNInstance *e, CNDB *db )
	{ return ((e) && !(e->sub[0]) && *(char*)e->sub[1]=='*' ); }
#else
char * DBIdentifier( CNInstance *, CNDB * );
int DBStarMatch( CNInstance *e, CNDB *db );
#endif

inline int isProxy( CNInstance *e )
	{ return ((e->sub[0]) && !e->sub[1]); }
inline CNEntity * DBProxyThis( CNInstance *proxy )
	{ return (CNEntity*) proxy->sub[0]->sub[0]; }
inline CNEntity * DBProxyThat( CNInstance *proxy )
	{ return (CNEntity*) proxy->sub[0]->sub[1]; }
inline int isProxySelf( CNInstance *proxy )
	{ return !DBProxyThis(proxy); }

//===========================================================================
//	i/o
//===========================================================================
int	db_outputf( FILE *, CNDB *, char *fmt, ... );

//===========================================================================
//	op (nil-based)
//===========================================================================

#include "db_op.h"


#endif	// DATABASE_H
