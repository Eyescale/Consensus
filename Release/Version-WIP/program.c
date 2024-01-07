#include <stdio.h>
#include <stdlib.h>

//#include "traverse.h"
#include "program.h"

// #define DEBUG

//===========================================================================
//	newProgram / freeProgram
//===========================================================================
CNProgram *
newProgram( CNStory *story, char *inipath )
{
	Pair *entry = CNStoryMain( story );
	if ( !entry ) {
		fprintf( stderr, "B%%: Error: story has no main\n" );
		return NULL; }
	CNCell *cell = newCell( entry, inipath );
	if ( !cell ) return NULL;
	CNProgram *program = (CNProgram *) newPair( story, newPair(NULL,NULL) );
	// threads->new is a list of lists
	program->threads->new = newItem( newItem( cell ) );
	return program;
}

void
freeProgram( CNProgram *program )
/*
	Note that in the normal course of execution both
	program->threads->active and program->threads->new
	should be NULL.
	Here we make provision to support other cases as well
*/
{
	if ( !program ) return;
	CNCell *cell;
	listItem **active = &program->threads->active;
	listItem **new = &program->threads->new;
	while (( cell = popListItem(active) )) {
		freeListItem( BMCellCarry(cell) );
		releaseCell( cell ); }
	while (( cell = popListItem(new) )) {
		freeListItem( BMCellCarry(cell) );
		releaseCell( cell ); }
	freePair((Pair *) program->threads );
	freePair((Pair *) program );
}

//===========================================================================
//	cnSync
//===========================================================================
void
cnSync( CNProgram *program )
{
#ifdef DEBUG
	fprintf( stderr, "cnSync: bgn\n" );
#endif
	if ( !program ) return;
	CNCell *cell;
	listItem *out = NULL;
	// update active cells
	listItem **active = &program->threads->active;
	for ( listItem *i=*active; i!=NULL; i=i->next ) {
		cell = i->ptr;
		if ( bm_cell_update( cell ) )
			addItem( &out, cell ); }
	// activate new cells
	listItem **new = &program->threads->new;
	for ( listItem *carry;( carry = popListItem(new) ); )
		while (( cell = popListItem(&carry) )) {
			bm_cell_init( cell );
			addItem( active, cell ); }
	// mark out cells
	while (( cell = popListItem(&out) ))
		bm_cell_mark_out( cell );
#ifdef DEBUG
	fprintf( stderr, "cnSync: end\n" );
#endif
}

//===========================================================================
//	cnOperate
//===========================================================================
int
cnOperate( CNProgram *program, CNCell **this )
{
#ifdef DEBUG
	fprintf( stderr, "cnOperate: bgn\n" );
#endif
	if ( !program ) return 0;

	CNCell *cell = *this;
	CNStory *story = program->story;
	if (( cell )) bm_cell_read( this, story );

	listItem **active = &program->threads->active;
	listItem **new = &program->threads->new;
	listItem *carry, *out=NULL;
	// operate active cells
	listItem *last_i = NULL, *next_i;
	for ( listItem *i=*active; i!=NULL; i=next_i ) {
		next_i = i->next;
		cell = i->ptr;
		if ( bm_cell_out(cell) ) {
			clipListItem( active, i, last_i, next_i );
			addItem( &out, cell ); }
		else {
			bm_cell_operate( cell, story );
			if (( carry = *BMCellCarry(cell) )) {
				addItem( new, carry );
				bm_cell_reset( cell ); }
			last_i = i; } }
	// release out cells
	while (( cell = popListItem(&out) ))
		releaseCell( cell );
#ifdef DEBUG
	fprintf( stderr, "cnOperate: end\n" );
#endif
	return ((*active)||(*new));
}

