#ifndef NARRATIVE_H
#define	NARRATIVE_H

#include "list.h"

typedef listItem CNStory;

CNStory *readStory( char *path );
int	story_output( FILE *, CNStory * );
void	freeStory( CNStory * );

typedef enum {
	ROOT,
	IN,
	ON,
	DO,
	EN,
	INPUT,
	OUTPUT,
	ELSE,
	ELSE_IN,
	ELSE_ON,
	ELSE_DO,
	ELSE_EN,
	ELSE_INPUT,
	ELSE_OUTPUT,
	LOCAL
} CNOccurrenceType;
#define PROTO 2020	// not an occurrence type

typedef struct {
	CNOccurrenceType type;
	struct {
		char *expression;
		listItem *sub;
	} *data;
} CNOccurrence;

typedef struct {
	char *proto;
	CNOccurrence *root;
} CNNarrative;


#endif	// NARRATIVE_H
