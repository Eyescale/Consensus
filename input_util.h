#ifndef INPUT_UTIL_H
#define INPUT_UTIL_H

/*---------------------------------------------------------------------------
	input_util actions		- public
---------------------------------------------------------------------------*/

_action	read_0;
_action	read_1;
_action	read_2;
_action	on_token;

/*---------------------------------------------------------------------------
	input_util utilities		- public
---------------------------------------------------------------------------*/

int	num_separators( void );
int	is_separator( int event );
int	string_start( IdentifierVA *string, int event );
int	string_append( IdentifierVA *string, int event );
char *	string_finish( IdentifierVA *string, int cleanup );
void	slist_append( listItem **slist, IdentifierVA *identifier, int event, int as_string );
void	slist_close( listItem **slist, IdentifierVA *identifier );

// unused
char *	string_extract( char *identifier );
char *	string_replace( char *expression, char *identifier, char *value );
listItem *stringify( listItem *string );


#endif	// INPUT_UTIL_H
