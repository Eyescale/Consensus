#ifndef PATH_H
#define PATH_H

/*---------------------------------------------------------------------------
	path actions		- public
---------------------------------------------------------------------------*/

_action	read_scheme;
_action	read_path;

/*---------------------------------------------------------------------------
	path utilities		- public
---------------------------------------------------------------------------*/

char *cn_path( _context *, const char *mode, char * );


#endif	// PATH_H
