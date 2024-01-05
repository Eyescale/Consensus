#ifndef STORY_H
#define	STORY_H

#include "registry.h"
#include "context.h"

//===========================================================================
//	Story types
//===========================================================================
typedef Registry CNStory;

typedef enum {
	BM_LOAD = 1,
	BM_INPUT,
	BM_STORY,
} BMReadMode;

//===========================================================================
//	Public Interface
//===========================================================================
CNStory *	readStory( char *path );
int		bm_load( char *path, BMContext *ctx );
char *		bm_read( FILE *stream );
void		freeStory( CNStory * );
int		cnStoryOutput( FILE *, CNStory * );

inline Pair *CNStoryMain( CNStory *story ) {
	if ( !story ) return NULL;
	Pair *entry = registryLookup( story, "" );
	return (( entry )&&( entry->value )) ?
		entry : NULL; }


#endif	// STORY_H
