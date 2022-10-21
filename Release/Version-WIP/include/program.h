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

typedef struct {
	Pair *entry; // into CNStory
	BMContext *ctx;
} CNCell;

CNProgram *newProgram( CNStory *, char *inipath );
void	freeProgram( CNProgram * );
int	cnOperate( CNProgram * );
void	cnUpdate( CNProgram * );

CNCell *newCell( Pair *, CNDB * );
void freeCell( CNCell * );


#endif	// PROGRAM_H
