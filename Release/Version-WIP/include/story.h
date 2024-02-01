#ifndef STORY_H
#define	STORY_H

#include "registry.h"
#include "arena.h"

//===========================================================================
//	Story types
//===========================================================================
typedef struct {
	Registry *narratives;
	CNArena *arena;
} CNStory;

//===========================================================================
//	Public Interface
//===========================================================================
CNStory *	readStory( char *path, int interactive );
int		cnStoryOutput( FILE *, CNStory * );
CNStory *	newStory( void );
void		freeStory( CNStory * );

static inline Pair *CNStoryMain( CNStory *story ) {
	if ( !story ) return NULL;
	Pair *entry = registryLookup( story->narratives, "" );
	// we have entry:[ "", {[ proto, root ]} ]
	return (( entry )&&( entry->value )) ? entry : NULL; }


#endif	// STORY_H
