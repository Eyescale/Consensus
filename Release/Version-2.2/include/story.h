#ifndef STORY_H
#define	STORY_H

#include "registry.h"
#include "registry.h"

//===========================================================================
//	Story types
//===========================================================================
typedef Registry CNStory;

//===========================================================================
//	Public Interface
//===========================================================================
CNStory *	newStory( void );
void		freeStory( CNStory * );
CNStory *	story_read( char *path, listItem * );
int		story_output( FILE *, CNStory * );

static inline Pair *CNStoryMain( CNStory *story ) {
	if ( !story ) return NULL;
	Pair *entry = registryLookup( story, "" );
	// we have entry:[ "", {[ proto, root ]} ]
	return (( entry )&&( entry->value )) ? entry : NULL; }


#endif	// STORY_H
