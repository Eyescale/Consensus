#ifndef CELL_H
#define CELL_H

#include "list.h"
#include "context.h"
#include "narrative.h"

typedef CNEntity CNCell;

CNCell * newCell( Pair *entry, CNEntity *parent );
void releaseCell( CNCell * );

inline Pair * BMCellEntry( CNCell *cell )
	{ return (Pair *) ((Pair *) cell->sub[ 0 ] )->name; }
inline listItem ** BMCellCarry( CNCell *cell )
	{ return (listItem **) &((Pair *) cell->sub[ 0 ] )->value; }
inline BMContext * BMCellContext( CNCell *cell )
	{ return (BMContext *) cell->sub[ 1 ]; }

inline void cellInit( CNCell *cell )
	{ bm_context_init( BMCellContext(cell) ); }
inline int cellUpdate( CNCell *cell )
	{ return bm_context_update( cell, BMCellContext(cell) ); }
int cellOperate( CNCell *, listItem **new, CNStory * );


#endif	// CELL_H
