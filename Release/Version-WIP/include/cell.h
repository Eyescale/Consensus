#ifndef CELL_H
#define CELL_H

#include "list.h"
#include "context.h"
#include "narrative.h"

typedef CNEntity CNCell;

CNCell * newCell( Pair *entry, char *inipath );
void releaseCell( CNCell * );

int bm_cell_operate( CNCell *, listItem **new, CNStory * );
CNInstance *bm_cell_carry( CNCell *, CNCell *, int );

inline Pair * BMCellEntry( CNCell *cell )
	{ return (Pair *) ((Pair *) cell->sub[ 0 ] )->name; }
inline listItem ** BMCellCarry( CNCell *cell )
	{ return (listItem **) &((Pair *) cell->sub[ 0 ] )->value; }
inline BMContext * BMCellContext( CNCell *cell )
	{ return (BMContext *) cell->sub[ 1 ]; }

static inline void bm_cell_init( CNCell *cell ) {
	bm_context_init( BMCellContext(cell) ); }
static inline int bm_cell_update( CNCell *cell ) {
	return bm_context_update( cell, BMCellContext(cell) ); }


#endif	// CELL_H
