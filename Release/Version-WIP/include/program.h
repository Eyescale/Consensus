#ifndef PROGRAM_H
#define PROGRAM_H

#include "story.h"
#include "cell.h"

typedef struct {
	CNStory *story;
	struct {
		listItem *active;
		listItem *new;
	} *threads;
} CNProgram;

int cnPrintOut( FILE *, char *path, int level );
CNProgram *newProgram( CNStory *, char *inipath );
void freeProgram( CNProgram * );
void cnSync( CNProgram * );
int cnOperate( CNCell **, CNProgram * );
void cnExit( int debug );

static inline CNCell * CNProgramStem( CNProgram *p ) {
	return ((listItem*) p->threads->new->ptr )->ptr; }


#endif	// PROGRAM_H
