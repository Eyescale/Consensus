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

//===========================================================================
//	data
//===========================================================================
#define isProxy( e )		((e->sub[0]) && !e->sub[1])
#define isProxySelf( proxy )	(DBProxyThis(proxy)==NULL)
#define DBProxyThis( proxy )	((CNEntity*) (proxy)->sub[0]->sub[0])
#define DBProxyThat( proxy )	((CNEntity*) (proxy)->sub[0]->sub[1])

CNInstance *	db_register( char *identifier, CNDB * );
int		db_match( CNDB *, CNInstance *, CNDB *, CNInstance * );
CNInstance *	db_proxy( CNEntity *, CNEntity *, CNDB * );
CNInstance *	db_instantiate( CNInstance *, CNInstance *, CNDB * );
CNInstance *	db_assign( CNInstance *, CNInstance *, CNDB * );
CNInstance *	db_unassign( CNInstance *, CNDB * );
void		db_deprecate( CNInstance *, CNDB * );
CNInstance *	db_lookup( int privy, char *identifier, CNDB * );
CNInstance *	db_first( CNDB *, listItem ** );
CNInstance *	db_next( CNDB *, CNInstance *, listItem ** );
int		db_traverse( int privy, CNDB *, DBTraverseCB, void * );

#define UNIFIED

#ifdef UNIFIED
#define db_identifier( e, db )	((char *) e->sub[1] )
#else
char * db_identifier( CNInstance *, CNDB * );
#endif

//===========================================================================
//	i/o
//===========================================================================
int	db_outputf( FILE *, CNDB *, char *fmt, ... );

//===========================================================================
//	op (nil-based)
//===========================================================================

#include "db_op.h"


#endif	// DATABASE_H
