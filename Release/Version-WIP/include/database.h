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
CNInstance *	db_star( CNDB * );
CNInstance *	db_register( char *identifier, CNDB * );
CNInstance *	db_proxy( CNEntity *, CNEntity *, CNDB * );
CNInstance *	db_instantiate( CNInstance *, CNInstance *, CNDB * );
int		db_manifested( CNInstance *, CNDB * );
CNInstance *	db_assign( CNInstance *, CNInstance *, CNDB * );
CNInstance *	db_unassign( CNInstance *, CNDB * );
void		db_deprecate( CNInstance *, CNDB * );
int		db_deprecated( CNInstance *, CNDB * );
void		db_signal( CNInstance *, CNDB * );
CNInstance *	db_lookup( int privy, char *identifier, CNDB * );
CNInstance *	db_first( CNDB *, listItem ** );
CNInstance *	db_next( CNDB *, CNInstance *, listItem ** );
int		db_traverse( int privy, CNDB *, DBTraverseCB, void * );

#ifdef UNIFIED
#define db_identifier( e, db )	((char *) e->sub[1] )
#else
char * db_identifier( CNInstance *, CNDB * );
#endif

//===========================================================================
//	op (nil-based)
//===========================================================================
CNInstance * db_log( int first, int released, CNDB *, listItem ** );
void db_update( CNDB *, CNInstance * );
int db_private( int privy, CNInstance *, CNDB * );
int db_still( CNDB * );
void db_init( CNDB * );
void db_exit( CNDB * );
int db_in( CNDB * );
int db_out( CNDB * );

//===========================================================================
//	i/o
//===========================================================================
int	db_outputf( FILE *, CNDB *, char *fmt, ... );


#endif	// DATABASE_H
