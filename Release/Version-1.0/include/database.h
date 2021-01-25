#ifndef DATABASE_H
#define DATABASE_H

#include "entity.h"
#include "registry.h"

typedef CNEntity CNInstance;
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
typedef enum {
	DB_CONDITION,
	DB_INSTANTIATED,
	DB_RELEASED
} DBLogType;

int	db_feel( char *expression, DBLogType, CNDB * );
CNInstance * db_lookup( int privy, char *identifier, CNDB * );
CNInstance * db_register( char *identifier, CNDB * );
listItem * db_couple( listItem *sub[2], CNDB * );
void	db_deprecate( CNInstance *, CNDB * );

//===========================================================================
//	utilities
//===========================================================================
int	db_private( int privy, CNInstance *, CNDB * );

int	output( FILE *, CNInstance *, CNDB * );
void	dbg_out( char *pre, CNInstance *e, char *post, CNDB * );



#endif	// DATABASE_H
