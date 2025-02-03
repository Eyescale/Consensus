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

typedef struct {
	int interactive;
	CNCell *this;
	listItem *flags;
} ProgramData;

void cnInit( void );
void cnExit( int report );

CNProgram *newProgram( char *inipath, CNStory **, ProgramData * );
void	freeProgram( CNProgram *, ProgramData * );
void	cnSync( CNProgram * );
int	cnOperate( CNProgram *, ProgramData * );
int	cnPrintOut( FILE *, char *path, listItem * );

#endif	// PROGRAM_H
