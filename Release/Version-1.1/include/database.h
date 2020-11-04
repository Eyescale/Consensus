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

CNDB *	newCNDB( void );
void	freeCNDB( CNDB * );

//===========================================================================
//	engine
//===========================================================================
void	db_update( CNDB * );

//===========================================================================
//	operation
//===========================================================================
CNInstance * db_register( char *identifier, CNDB * );
listItem * db_couple( listItem *sub[2], CNDB * );
void	db_deprecate( CNInstance *, CNDB * );
void	db_exit( CNDB * );

//===========================================================================
//	utilities
//===========================================================================
int	db_is_empty( CNDB * );
CNInstance * db_first( CNDB *, listItem ** );
CNInstance * db_next( CNDB *, CNInstance *, listItem ** );
CNInstance * db_log( int first, int released, CNDB *, listItem ** );
CNInstance * db_lookup( int privy, char *identifier, CNDB * );
int	db_private( int privy, CNInstance *, CNDB * );
int	db_out( CNDB * );

int	cn_out( FILE *, CNInstance *, CNDB * );
void	dbg_out( char *pre, CNInstance *e, char *post, CNDB * );



#endif	// DATABASE_H
