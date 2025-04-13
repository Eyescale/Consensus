#ifndef OPERATION_H
#define OPERATION_H

#include "cell.h"
#include "context.h"
#include "narrative.h"
#include "story.h"

void bm_operate( CNNarrative *, BMContext *, CNStory *,
	listItem *narratives, Registry *subs );

void bm_op( int type, char *, BMContext *, CNStory * );
int bm_vop( int type, ... );

#endif	// OPERATION_H
