#ifndef API_H
#define API_H

/*---------------------------------------------------------------------------
	API 		- public
---------------------------------------------------------------------------*/

int	cn_dof( const char *format, ... );
int	cn_do( char *action );
int	cn_dob( listItem *actions, int level );
Entity	*cn_setf( char *format, ... );

#ifdef DO_LATER
listItem *cn_getf( char *format, ... );
int	cn_testf( listItem **results, char *format, ... );
#else
Entity	*cn_getf( const char *format, ... );	// to be deprecated
Entity	*cn_instf( const char *format, ... );	// to be deprecated
#endif

Entity	*cn_new( char *name );
Entity	*cn_instantiate( Entity *source, Entity *medium, Entity *target, int pointback );
int	cn_activate( Entity *e );
int	cn_deactivate( Entity *e );
void	cn_release( Entity *e );

Expression *cn_expression( Entity *e );
Entity	*cn_entity( char *name );
char	*cn_name( Entity *e );
Entity	*cn_instance( Entity *source, Entity *medium, Entity *target );
int	cn_is_active( Entity *e );
void	cn_free( Entity *e );

void	*cn_va_get( Entity *e, char *va_name );
registryEntry *cn_va_set( Entity *e, char *va_name, void *value );

int	cn_instantiate_narrative( Entity *e, Narrative *n );
Narrative *cn_activate_narrative( Entity *e, char *name );
int	cn_deactivate_narrative( Entity *e, char *name );
int	cn_release_narrative( Entity *e, char *name );

int	cn_open( char *path, int oflags );

#endif	// API_H
