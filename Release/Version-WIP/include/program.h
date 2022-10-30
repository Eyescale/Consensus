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

typedef CNEntity CNCell;

CNProgram *newProgram( CNStory *, char *inipath );
void	freeProgram( CNProgram * );
int	cnOperate( CNProgram * );
void	cnUpdate( CNProgram * );

CNCell *newCell( Pair *, CNEntity * );

#define BMCellEntry( cell )	((Pair *) ((Pair *) cell->sub[0])->name )
#define BMCellCarry( cell )	((listItem **) &((Pair *)cell->sub[0])->value )
#define BMCellContext( cell )	((BMContext *)(cell)->sub[ 1 ])

#endif	// PROGRAM_H
