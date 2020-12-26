#ifndef OPERATION_H
#define OPERATION_H

#include "database.h"
#include "narrative.h"

void	cnLoad( char *path, CNDB * );
int	cnOperate( CNStory *, CNDB * );
void	cnUpdate( CNDB * );


#endif	// OPERATION_H
