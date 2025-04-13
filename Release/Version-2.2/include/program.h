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
	union { int on; CNCell *this; } ui;
	listItem *flags;
} ProgramData;

ProgramData * cnInit( int *, char *** );
void cnExit( int report, ProgramData * );
void cnProgramSetInteractive( ProgramData * );

CNProgram *newProgram( char *inipath, CNStory **, ProgramData * );
void	freeProgram( CNProgram *, ProgramData * );
void	cnSync( CNProgram * );
int	cnOperate( CNProgram *, ProgramData * );
int	cnPrintOut( FILE *, char *path, ProgramData * );

static inline CNStory *
readStory( char *path, ProgramData *data ) {
	return story_read( path, data->flags ); }

static inline void
cnStoryOutput( FILE *stream, CNStory *story ) {
	story_output( stream, story ); }

#endif	// PROGRAM_H
