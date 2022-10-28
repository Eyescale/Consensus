#ifndef ASSIGNMENT_H
#define	ASSIGNMENT_H

#include "narrative.h"
#include "traverse.h"
#include "query.h"
#include "proxy.h"

void bm_instantiate_assignment( char *, BMTraverseData *, CNStory * );
CNInstance * bm_query_assignment( BMQueryType, char *, BMQueryData * );
CNInstance * bm_proxy_feel_assignment( CNInstance *, char *, BMTraverseData * );


#endif	// ASSIGNMENT_H
