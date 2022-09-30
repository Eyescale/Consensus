#ifndef NARRATIVE_H
#define	NARRATIVE_H

#include "registry.h"

//===========================================================================
//	Occurrence, Narrative, Story types
//===========================================================================
// Occurrence types

#define ROOT		0
#define	IN		1
#define ON		2
#define DO		4
#define EN		8
#define INPUT		16
#define OUTPUT		32
#define LOCALE		64

#define ELSE		256
#define ELSE_IN		(ELSE|IN)
#define ELSE_ON		(ELSE|ON)
#define ELSE_DO		(ELSE|DO)
#define ELSE_EN		(ELSE|EN)
#define ELSE_INPUT	(ELSE|INPUT)
#define ELSE_OUTPUT	(ELSE|OUTPUT)

typedef enum {
	CN_LOAD = 1,
	CN_INPUT,
	CN_STORY,
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


#endif	// NARRATIVE_H
