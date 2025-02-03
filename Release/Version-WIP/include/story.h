#ifndef STORY_H
#define	STORY_H

#include "registry.h"

//===========================================================================
//	Story types
//===========================================================================
typedef Registry CNStory;

//===========================================================================
//	Public Interface
//===========================================================================
CNStory *	readStory( char *path, listItem *flags );
int		cnStoryOutput( FILE *, CNStory * );
CNStory *	newStory( void );
void		freeStory( CNStory * );

static inline Pair *CNStoryMain( CNStory *story ) {
	if ( !story ) return NULL;
	Pair *entry = registryLookup( story, "" );
	// we have entry:[ "", {[ proto, root ]} ]
	return (( entry )&&( entry->value )) ? entry : NULL; }


#endif	// STORY_H
