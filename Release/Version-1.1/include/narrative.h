#ifndef NARRATIVE_H
#define	NARRATIVE_H

#include "list.h"

typedef enum {
	ROOT,
	IN,
	ON,
	DO,
	INPUT,
	OUTPUT,
	ELSE,
	ELSE_IN,
	ELSE_ON,
	ELSE_DO,
	ELSE_INPUT,
	ELSE_OUTPUT
} CNOccurrenceType;

typedef struct {
	CNOccurrenceType type;
	struct {
		char *expression;
		listItem *sub;
	} *data;
} CNOccurrence;

typedef struct {
	char *proto;
	CNOccurrence *base;
} CNNarrative;

typedef listItem CNStory;

CNStory * readStory( char *path );
int	story_output( FILE *, CNStory *, int );
void	freeStory( CNStory * );


#endif	// NARRATIVE_H
