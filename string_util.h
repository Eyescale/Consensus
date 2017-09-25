#ifndef STRING_UTIL_H
#define STRING_UTIL_H

/*---------------------------------------------------------------------------
	string utilities		- public
---------------------------------------------------------------------------*/

int	is_separator( int event );
int	string_start( IdentifierVA *string, int event );
int	string_append( IdentifierVA *string, int event );
char *	string_finish( IdentifierVA *string, int cleanup );
int	string_assign( char **target, int id, _context *context );
char *	string_extract( char *identifier );
char *	string_replace( char *expression, char *identifier, char *value );


#endif	// STRING_UTIL_H
