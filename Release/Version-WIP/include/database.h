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
int		db_match( CNInstance *, CNDB *, CNInstance *, CNDB * );
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

typedef struct {
	int type, first;
	CNInstance *last;
} OutputData;
static inline void
db_putout( CNInstance *e, CNDB *db, OutputData *data )
{
	if (( data->last )) {
		if ( data->first ) {
			printf( (data->type=='s') ? "\\{ " : "{ " );
			data->first = 0; }
		else printf( ", " );
		db_outputf( stdout, db, "%_", data->last ); }
	data->last = e;
}

inline int
db_flush( OutputData *data, CNDB *db )
{
	if ( data->first ) {
		char_s fmt;
		fmt.value = 0;
		sprintf( fmt.s, "%%%c", data->type );
		db_outputf( stdout, db, fmt.s, data->last ); }
	else {
		printf( ", " );
		db_outputf( stdout, db, "%_", data->last );
		printf( " }" ); }
	return 0;
}

//===========================================================================
//	op (nil-based)
//===========================================================================

#include "db_op.h"


#endif	// DATABASE_H
