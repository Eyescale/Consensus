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

_action	systemFrame;

/*---------------------------------------------------------------------------
	frame utilities	- public
---------------------------------------------------------------------------*/

int	test_log( _context *context, LogType type );


#endif	// FRAME_H
