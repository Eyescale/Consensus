#ifndef CELL_H
#define CELL_H

#include "list.h"
#include "context.h"
#include "narrative.h"
#include "story.h"

typedef CNEntity CNCell;

CNCell * newCell( CNStory *, Pair *entry );
void	freeCell( CNCell * );
void	bm_bond( CNEntity *, CNEntity *, CNInstance * );
int	bm_cell_input( CNCell *, CNStory * );
void	bm_cell_operate( CNCell *, CNStory * );

static inline Pair * BMCellEntry( CNCell *cell )
	{ return (Pair *) ((Pair *) cell->sub[ 0 ] )->name; }
static inline listItem ** BMCellCarry( CNCell *cell )
	{ return (listItem **) &((Pair *) cell->sub[ 0 ] )->value; }
static inline BMContext * BMCellContext( CNCell *cell )
	{ return (BMContext *) cell->sub[ 1 ]; }

static inline void bm_cell_init( CNCell *cell ) {
	bm_context_init( BMCellContext(cell) ); }
static inline int bm_cell_update( CNCell *cell, CNStory *story ) {
	return bm_context_update( BMCellContext(cell) ); }

#define bm_cell_mark_out(cell) \
	{ *BMCellCarry( cell ) = (void *) cell; }
#define bm_cell_out( cell ) \
	( *BMCellCarry( cell ) == (void *) cell )
#define bm_cell_reset( cell ) \
	{ *BMCellCarry( cell ) = NULL; }

#endif	// CELL_H
