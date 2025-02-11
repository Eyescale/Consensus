#ifndef DB_ARENA_H
#define DB_ARENA_H

void		db_arena_init( void );
void		db_arena_exit( void );
void		db_arena_encode( CNString *, CNString * );
void *		db_arena_key( char * );
CNInstance *	db_arena_register( char *, CNDB * );
CNInstance *	db_arena_lookup( char *, CNDB * );
CNInstance *	db_arena_makeup( CNInstance *, CNDB *, CNDB * );
void		db_arena_output( FILE *, char *, char * );
void		db_arena_deregister( CNInstance *, CNDB * );
char *		db_arena_identifier( CNInstance * );
CNInstance *	db_arena_translate( CNInstance *, CNDB *, int );
void		db_arena_unref( CNDB * );

#endif // DB_ARENA_H
