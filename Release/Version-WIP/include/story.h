#ifndef STORY_H
#define	STORY_H

#include "registry.h"
#include "context.h"

//===========================================================================
//	Story types
//===========================================================================
typedef struct {
	Registry *narratives;
	Registry *arena;
} CNStory;

//===========================================================================
//	Public Interface
//===========================================================================
CNStory *	readStory( char *path, int interactive );
int		bm_load( char *path, BMContext *ctx );
char *		bm_read( FILE *stream );
CNStory *	newStory( void );
void		freeStory( CNStory * );
int		cnStoryOutput( FILE *, CNStory * );
void *		newStoryRoot( CNStory * );

inline Pair *CNStoryMain( CNStory *story ) {
	if ( !story ) return NULL;
	Pair *entry = registryLookup( story->narratives, "" );
	// we have entry:[ "", {[ proto, root ]} ]
	return (( entry )&&( entry->value )) ? entry : NULL; }


#endif	// STORY_H
