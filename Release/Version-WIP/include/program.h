#ifndef PROGRAM_H
#define PROGRAM_H

#include "context.h"
#include "story.h"

typedef struct {
	CNStory *story;
	struct {
		listItem *active;
		listItem *new;
	} *threads;
} CNProgram;

CNProgram *newProgram( CNStory *, char *inipath );
void	freeProgram( CNProgram * );
void	cnSync( CNProgram * );
int	cnOperate( CNProgram * );


#endif	// PROGRAM_H
