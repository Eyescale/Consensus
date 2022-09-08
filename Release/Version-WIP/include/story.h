#ifndef STORY_H
#define	STORY_H

#include "registry.h"
#include "string_util.h"

//===========================================================================
//	Occurrence, Narrative, Story types
//===========================================================================
#define ROOT		0
#define	IN		1
#define ON		2
#define DO		4
#define EN		8
#define INPUT		16
#define OUTPUT		32

#define ELSE		256
#define ELSE_IN		(ELSE|IN)
#define ELSE_ON		(ELSE|ON)
#define ELSE_DO		(ELSE|DO)
#define ELSE_EN		(ELSE|EN)
#define ELSE_INPUT	(ELSE|INPUT)
#define ELSE_OUTPUT	(ELSE|OUTPUT)

#define LOCALE		512
#define COMPOUND	1024

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
int		cnStoryOutput( FILE *, CNStory * );


#endif	// STORY_H
