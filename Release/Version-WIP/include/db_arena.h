#ifndef DB_ARENA_H
#define DB_ARENA_H

#include "database.h"

typedef CNInstance * DBAQueryCB( CNInstance *, CNDB *, char *, void * );

void		db_arena_init( void );
void		db_arena_exit( void );
CNInstance *	db_arena_register( char *, CNDB * );
void		db_arena_deregister( CNInstance *, CNDB * );
void		db_arena_encode( CNString *, CNString *, Pair * );
char *		db_arena_origin( CNInstance * );
void *		db_arena_key( char * );
CNInstance *	db_arena_lookup( char *, CNDB * );
CNInstance *	db_arena_makeup( CNInstance *, CNDB *, CNDB * );
void		db_arena_output( FILE *, char *, char * );
char *		db_arena_identifier( CNInstance * );
CNInstance *	db_arena_translate( CNInstance *, CNDB *, int );
void		db_arena_unref( CNDB * );
CNInstance *	db_arena_query( CNDB *, char *, DBAQueryCB, void * );

static inline int db_arena_verify( uint *master, char *code ) {
	if ( !master ) return 0;
	union { uint value[2]; void *ptr; } key;
	key.ptr = db_arena_key( code );
	return ( master[0]==key.value[0] && master[1]==key.value[1] ); }


#endif // DB_ARENA_H
