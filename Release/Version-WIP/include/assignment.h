#ifndef ASSIGNMENT_H
#define	ASSIGNMENT_H

#include "narrative.h"
#include "traverse.h"
#include "query.h"

void bm_instantiate_assignment( char *, BMTraverseData *, CNStory * );
CNInstance * bm_query_assignment( BMQueryType, char *, BMQueryData * );


#endif	// ASSIGNMENT_H
