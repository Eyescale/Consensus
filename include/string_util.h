#ifndef STRING_UTIL_H
#define STRING_UTIL_H

int	isanumber( char *string );

/*---------------------------------------------------------------------------
	string_util	utilities
---------------------------------------------------------------------------*/

StringVA *newString( void );
void	freeString( StringVA * );

int	string_start( StringVA *string, int event );
int	string_append( StringVA *string, int event );
char *	string_finish( StringVA *string, int cleanup );
listItem *string_expand( char *string, char **next_pos );

/*
	The "stringify" utility functions allow to add an event (char)
	to an existing StringVA (or text line). This event will either
		. if it is not '\n':
			be added to the string in progress
		. if it is '\n':
			be optionally added to the string in progress, then
			close the current string, which, if it is not made
			solely of "\n", will be added to the list.

	The 'string in progress' is a pointer to an StringVA held by the caller.
	stringify() is not used, but provided as an example implementation.
*/

void	slist_append( listItem **slist, StringVA *string, int event, int nocr, int cleanup );
void	slist_close( listItem **slist, StringVA *string, int cleanup );
listItem *stringify( listItem *string, int cleanup );

/*---------------------------------------------------------------------------
	unused
---------------------------------------------------------------------------*/

char *	string_extract( char *identifier );
char *	string_replace( char *expression, char *identifier, char *value );

/*
   The functions below proceed from the view:

	A Text is a list of lines
	A Line is a list of segments
	A segment is a list of options
	An option is a string (registryEntry *), where
		. the string->index.name is the text itself (char *)
		. the string->value is the list of events (char) preluding
		  to the construction of the text itself

	see outputLines() for example usage
*/

listItem *addLine( listItem **lines );
listItem *addSegment( listItem *lines );
listItem *addOption( listItem *lines, int nostr );
void *popOption( listItem *lines );
void *popSegment( listItem *lines );
void *popLine( listItem **lines );

void freeLines( listItem **lines, int nostr );
void outputLines( listItem *lines );

#endif	// STRING_UTIL_H
