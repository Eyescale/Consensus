#ifndef STORY_H
#define	STORY_H

#include "registry.h"
#include "string_util.h"

//===========================================================================
//	Occurrence, Narrative, Story types
//===========================================================================
#define ROOT		 0
#define	IN		 1
#define ON		 2
#define DO		 4
#define INPUT		 8
#define OUTPUT		16
#define ELSE	       256
#define ELSE_IN	       257
#define ELSE_ON	       258
#define ELSE_DO	       260
#define ELSE_INPUT     264
#define ELSE_OUTPUT    272

#define COMPOUND 1024

typedef enum {
	CN_INI = 1,
	CN_STORY,
	CN_INSTANCE,
} BMReadMode;
void *	bm_read( BMReadMode, ... );

typedef struct {
	struct {
		int type;
		char *expression;
	} *data;
	listItem *sub;
} CNOccurrence;

typedef struct {
	char *proto;
	CNOccurrence *root;
} CNNarrative;

typedef Registry CNStory;

//===========================================================================
//	Public Interface
//===========================================================================
CNStory *	readStory( char *path );
void		freeStory( CNStory * );
CNNarrative *	newNarrative( void );
void		freeNarrative( CNNarrative * );
CNOccurrence *	newOccurrence( int );
void		freeOccurrence( CNOccurrence * );

int	story_add( CNStory *, CNNarrative * );
int	story_output( FILE *, CNStory * );
int	proto_set( CNNarrative *, CNStory *, CNString * );

inline CNNarrative *
storyLookup( CNStory *story, char *n ) {
	return ((CNNarrative *) registryLookup( story, n ));
}


#endif	// STORY_H
