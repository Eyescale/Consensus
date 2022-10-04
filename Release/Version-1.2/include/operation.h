#ifndef OPERATION_H
#define OPERATION_H

#include "program.h"

typedef struct {
	Registry *subs;
	listItem *connect;
} BMCall;

int	bm_operate( CNCell *, listItem **new, CNStory * );
void	bm_update( BMContext *, int new );


#endif	// OPERATION_H
