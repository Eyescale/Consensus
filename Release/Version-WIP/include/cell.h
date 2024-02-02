#ifndef CELL_H
#define CELL_H

#include "list.h"
#include "context.h"
#include "narrative.h"
#include "story.h"

typedef CNEntity CNCell;
typedef void freeCellCB( CNCell *, void * );

int		bm_cell_read( CNCell **, CNStory * );
CNCell *	newCell( Pair *entry, char *inipath );
void		freeCell( CNCell *, freeCellCB, void * );
void		bm_cell_operate( CNCell *, CNStory * );
CNInstance *	bm_intake( BMContext *, CNDB *, CNInstance *, CNDB * );
CNInstance *	bm_inform( BMContext *, CNInstance *, CNDB * );
listItem *	bm_list_inform( BMContext *, listItem **, CNDB *, int * );
void		bm_cell_bond( CNCell *, CNCell *, CNInstance * );

static inline Pair * BMCellEntry( CNCell *cell )
	{ return (Pair *) ((Pair *) cell->sub[ 0 ] )->name; }
static inline listItem ** BMCellCarry( CNCell *cell )
	{ return (listItem **) &((Pair *) cell->sub[ 0 ] )->value; }
static inline BMContext * BMCellContext( CNCell *cell )
	{ return (BMContext *) cell->sub[ 1 ]; }
static inline BMContext * BMMakeCurrent( CNCell *cell ) {
	BMContext *ctx = BMCellContext(cell);
	BMContextCurrent(ctx)->name = BMContextSelf(ctx);
	return ctx; }

#define bm_cell_mark_out(cell) \
	{ *BMCellCarry( cell ) = (void *) cell; }
#define bm_cell_out( cell ) \
	( *BMCellCarry( cell ) == (void *) cell )
#define bm_cell_reset( cell ) \
	{ *BMCellCarry( cell ) = NULL; }

static inline void bm_cell_init( CNCell *cell ) {
	bm_context_init( BMCellContext(cell) ); }
static inline int bm_cell_update( CNCell *cell, CNStory *story ) {
	return bm_context_update( BMCellContext(cell), story ); }



#endif	// CELL_H
