#ifndef DATABASE_H
#define DATABASE_H

#include "registry.h"

typedef struct _Entity {
	struct _Entity **sub;
	listItem **as_sub;
} CNInstance;

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
CNInstance *	db_register( char *identifier, CNDB *, int notify );
CNInstance *	db_couple( CNInstance *, CNInstance *, CNDB * );
CNInstance *	db_instantiate( CNInstance *, CNInstance *, CNDB * );
void		db_deprecate( CNInstance *, CNDB * );
CNInstance *	db_lookup( int privy, char *identifier, CNDB * );
char *		db_identifier( CNInstance *, CNDB * );
int		db_is_empty( int privy, CNDB * );
CNInstance *	db_first( CNDB *, listItem ** );
CNInstance *	db_next( CNDB *, CNInstance *, listItem ** );
int		db_traverse( int privy, CNDB *, DBTraverseCB, void * );
void		db_exit( CNDB * );

//===========================================================================
//	op (nil-based)
//===========================================================================
CNInstance * db_log( int first, int released, CNDB *, listItem ** );
void db_update( CNDB * );
int db_private( int privy, CNInstance *, CNDB * );
int db_still( CNDB * );
int db_out( CNDB * );

//===========================================================================
//	i/o
//===========================================================================
char *	db_input( FILE *, int authorized );
int	db_output( FILE *, char *format, CNInstance *, CNDB * );


#endif	// DATABASE_H
