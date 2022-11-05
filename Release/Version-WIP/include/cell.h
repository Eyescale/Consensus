#ifndef CELL_H
#define CELL_H

#include "list.h"
#include "context.h"
#include "narrative.h"

typedef CNEntity CNCell;

CNCell * newCell( Pair *entry, CNEntity *parent );
void releaseCell( CNCell * );

#define cellInit( cell ) bm_context_init( BMCellContext(cell) )
#define cellUpdate( cell ) bm_context_update( cell, BMCellContext(cell) )
int cellOperate( CNCell *, listItem **new, CNStory * );

#define BMCellEntry( cell )	((Pair *) ((Pair *) cell->sub[0])->name )
#define BMCellCarry( cell )	((listItem **) &((Pair *)cell->sub[0])->value )
#define BMCellContext( cell )	((BMContext *)(cell)->sub[ 1 ])


#endif	// CELL_H
