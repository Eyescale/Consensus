#ifndef PROGRAM_H
#define PROGRAM_H

#include "story.h"
#include "context.h"

typedef struct {
	CNStory *story;
	struct {
		listItem *active;
		listItem *new;
	} *threads;
} CNProgram;

typedef struct {
	CNNarrative *narrative;
	BMContext *ctx;
} CNCell;

CNProgram *newProgram( CNStory *, char *inipath );
void	freeProgram( CNProgram * );
int	cnOperate( CNProgram * );
void	cnUpdate( CNProgram * );


#endif	// PROGRAM_H
