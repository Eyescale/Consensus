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
//	operations
//===========================================================================
CNInstance *	db_register( char *identifier, CNDB *, int manifest );
CNInstance *	db_instantiate( CNInstance *, CNInstance *, CNDB * );
int		db_coupled( int privy, CNInstance *, CNDB * );
void		db_uncouple( CNInstance *, CNDB * );
void		db_deprecate( CNInstance *, CNDB * );
void		db_signal( CNInstance *, CNDB * );
CNInstance *	db_lookup( int privy, char *identifier, CNDB * );
char *		db_identifier( CNInstance *, CNDB * );
int		db_is_empty( int privy, CNDB * );
CNInstance *	db_first( CNDB *, listItem ** );
CNInstance *	db_next( CNDB *, CNInstance *, listItem ** );
int		db_traverse( int privy, CNDB *, DBTraverseCB, void * );

//===========================================================================
//	op (nil-based)
//===========================================================================
CNInstance * db_log( int first, int released, CNDB *, listItem ** );
void db_update( CNDB * );
int db_private( int privy, CNInstance *, CNDB * );
int db_still( CNDB * );
void db_init( CNDB * );
void db_exit( CNDB * );
int db_in( CNDB * );
int db_out( CNDB * );

//===========================================================================
//	i/o
//===========================================================================
int	db_output( FILE *, char *format, CNInstance *, CNDB * );
int	db_outputf( FILE *, char *format, ... );


#endif	// DATABASE_H
