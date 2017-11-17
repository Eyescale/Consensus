#ifndef STRING_UTIL_H
#define STRING_UTIL_H

// utilities

listItem *string_expand( char *string, char **next_pos );

/*---------------------------------------------------------------------------

	A StringList is a list of strings
	A string is a list of segments
	A segment is a list of options
	An option is a line of text (aka. IdentifierVA) where
		. the identifier->value is a list of events (char) preluding
		  to the construction of the text itself
		. the identifier->name is the text itself (char *)

---------------------------------------------------------------------------*/

IdentifierVA *newIdentifier( void );
void	freeIdentifier( IdentifierVA * );
/*
	The "stringify" utility functions allow to add an event (char)
	to an existing IdentifierVA (or text line). This event will either
		. if it is not '\n:
			be added to the identifier in progress
		. if it is '\n':
			be optionally added to the identifier in progress, then
			close the current identifier, which, if it is not made
			solely of "\n", will be added to the list.

	The 'identifier in progress' is a pointer to an IdentifierVA held by the caller.
	stringify() is not used, but provided as an example implementation.
*/
void	slist_append( listItem **slist, IdentifierVA *string, int event, int nocr, int cleanup );
void	slist_close( listItem **slist, IdentifierVA *string, int cleanup );
listItem *stringify( listItem *string, int cleanup );

/*
	string_start resets the whole string - removing all segments
	string_append appends the character event to the latest text
		option of the latest segment of the passed string
	string_finish converts the latest text's characters (value)
		into a char * (name) and frees the character list
*/
int	string_start( IdentifierVA *string, int event );
int	string_append( IdentifierVA *string, int event );
char *	string_finish( IdentifierVA *string, int cleanup );

// unused

char *	string_extract( char *identifier );
char *	string_replace( char *expression, char *identifier, char *value );


#endif	// STRING_UTIL_H
