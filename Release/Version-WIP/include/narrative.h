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
#define ON_X		4
#define DO		8
#define EN		16
#define INPUT		32
#define OUTPUT		64
#define LOCALE		128

#define ELSE		512
#define ELSE_IN		(ELSE|IN)
#define ELSE_ON		(ELSE|ON)
#define ELSE_ON_X	(ELSE|ON_X)
#define ELSE_DO		(ELSE|DO)
#define ELSE_EN		(ELSE|EN)
#define ELSE_INPUT	(ELSE|INPUT)
#define ELSE_OUTPUT	(ELSE|OUTPUT)

typedef enum {
	BM_LOAD = 1,
	BM_INPUT,
	BM_STORY,
} BMReadMode;

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
void *		bm_read( BMReadMode, ... );


#endif	// NARRATIVE_H
