#ifndef NARRATIVE_H
#define	NARRATIVE_H

#include "registry.h"

//===========================================================================
//	Occurrence, Narrative, Story types
//===========================================================================
// Occurrence types

#define ROOT		0
#define	IN		1
#define ON		(1<<1)
#define ON_X		(1<<2)
#define DO		(1<<3)
#define EN		(1<<4)
#define ELSE		(1<<5)
#define INPUT		(1<<6)
#define OUTPUT		(1<<7)
#define LOCALE		(1<<8)
#define PER		(1<<9)
#define PER_X		(ON_X|PER)

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

inline Pair *CNStoryMain( CNStory *story ) {
	if ( !story ) return NULL;
	Pair *entry = registryLookup( story, "" );
	return (( entry )&&( entry->value )) ?
		entry : NULL; }

CNNarrative *	newNarrative( void );
void		freeNarrative( CNNarrative * );


#endif	// NARRATIVE_H
