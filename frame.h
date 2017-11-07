#ifndef FRAME_H
#define FRAME_H

typedef enum {
	OCCURRENCES,
	THEN,
	EVENTS,
	ACTIONS
} LogType;

/*---------------------------------------------------------------------------
	frame actions	- public
---------------------------------------------------------------------------*/

_action	frame_update;

/*---------------------------------------------------------------------------
	frame utilities	- public
---------------------------------------------------------------------------*/

int	test_log( _context *context, LogType type );
void	free_log( FrameLog *log );
void	narrative_process( Narrative *n, FrameLog *log, _context *context );


#endif	// FRAME_H
