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

void cnInit( void );
void cnExit( int report );

CNProgram *newProgram( CNStory *, char *inipath );
void	freeProgram( CNProgram * );
int	cnPrintOut( FILE *, char *path, int level );
void	cnSync( CNProgram * );
int	cnOperate( CNCell **, CNProgram * );

static inline CNCell * CNProgramStem( CNProgram *p ) {
	return ((listItem*) p->threads->new->ptr )->ptr; }


#endif	// PROGRAM_H
