#ifndef PROGRAM_H
#define PROGRAM_H

#include "context.h"
#include "narrative.h"

typedef struct {
	CNStory *story;
	struct {
		listItem *active;
		listItem *new;
	} *threads;
} CNProgram;

CNProgram *newProgram( CNStory *, char *inipath );
void	freeProgram( CNProgram * );
int	cnOperate( CNProgram * );
void	cnUpdate( CNProgram * );


#endif	// PROGRAM_H
