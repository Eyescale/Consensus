#ifndef INPUT_UTIL_H
#define INPUT_UTIL_H

#include "string_util.h"

/*---------------------------------------------------------------------------
	input_util actions		- public
---------------------------------------------------------------------------*/

_action	read_0;
_action	read_1;
_action	read_2;
_action	read_token;

/*---------------------------------------------------------------------------
	input_util utilities		- public
---------------------------------------------------------------------------*/

int	test_identifier( _context *context, int i );
void	set_identifier( _context *context, int i, char *value );
char *	get_identifier( _context *context, int i );
char *	take_identifier( _context *context, int i );
void	free_identifier( _context *context, int i );

int	num_separators( void );
int	is_separator( int event );
int	string_start( IdentifierVA *string, int event );
int	string_append( IdentifierVA *string, int event );
char *	string_finish( IdentifierVA *string, int cleanup );
listItem *string_expand( char *string, char **next_pos );
void	slist_append( listItem **slist, IdentifierVA *identifier, int event, int as_string, int cleanup );
void	slist_close( listItem **slist, IdentifierVA *identifier, int cleanup );

// unused
char *	string_extract( char *identifier );
char *	string_replace( char *expression, char *identifier, char *value );
listItem *stringify( listItem *string, int cleanup );


#endif	// INPUT_UTIL_H
