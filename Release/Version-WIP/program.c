#include <stdio.h>
#include <stdlib.h>

#include "traverse.h"
#include "program.h"
#include "cell.h"

// #define DEBUG

//===========================================================================
//	newProgram / freeProgram
//===========================================================================
CNProgram *
newProgram( CNStory *story, char *inipath )
{
	if ( !story ) return NULL;
	Pair *entry = registryLookup( story, "" );
	if ( !entry || !entry->value ) {
		fprintf( stderr, "B%%: Error: story has no main\n" );
		return NULL; }
	CNCell *cell = newCell( entry, NULL );
	if ( !cell ) {
		fprintf( stderr, "B%%: Error: unable to allocate Cell\n" );
		return NULL; }
	if (( inipath )) {
		union { void *ptr; int value; } errnum;
		errnum.ptr = bm_read( BM_LOAD, BMCellContext(cell), inipath );
		if ( errnum.value ) {
			fprintf( stderr, "B%%: Error: load init file: '%s' failed\n", inipath );
			releaseCell( cell ); return NULL; } }
	// threads->new is a list of lists
	Pair *threads = newPair( NULL, newItem(newItem(cell)) );
	return (CNProgram *) newPair( story, threads );
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
//	cnUpdate
//===========================================================================
void
cnUpdate( CNProgram *program )
{
#ifdef DEBUG
	fprintf( stderr, "cnUpdate: bgn\n" );
#endif
	if ( !program ) return;
	listItem **active = &program->threads->active;
	listItem **new = &program->threads->new;
	listItem *carry, *out = NULL;
	CNCell *cell;
	// update active cells
	for ( listItem *i=*active; i!=NULL; i=i->next ) {
		cell = i->ptr;
		if (( carry = *BMCellCarry(cell) )) {
			*BMCellCarry( cell ) = NULL;
			addItem( new, carry ); }
		if ( !!cellUpdate(cell) )
			addItem( &out, cell ); }
	// activate new cells
	while (( carry = popListItem(new) ))
		while (( cell = popListItem(&carry) )) {
			addItem( active, cell );
			cellInit( cell ); }
	// mark exiting cells
	while (( cell = popListItem(&out) ))
		*BMCellCarry( cell ) = (void *) cell;
#ifdef DEBUG
	fprintf( stderr, "cnUpdate: end\n" );
#endif
}

//===========================================================================
//	cnOperate
//===========================================================================
int
cnOperate( CNProgram *program )
{
#ifdef DEBUG
	fprintf( stderr, "cnOperate: bgn\n" );
#endif
	if ( !program ) return 0;
	CNCell *cell;
	CNStory *story = program->story;
	listItem **active = &program->threads->active;
	listItem **new = &program->threads->new;
	listItem *released = NULL;
	// operate active cells
	listItem *last_i = NULL, *next_i;
	for ( listItem *i=*active; i!=NULL; i=next_i ) {
		next_i = i->next;
		cell = i->ptr;
		if ( !cellOperate( cell, new, story ) ) {
			addItem( &released, cell );
			clipListItem( active, i, last_i, next_i ); }
		else last_i = i; }
	// release deactivated cells
	while (( cell = popListItem(&released) ))
		releaseCell( cell );
#ifdef DEBUG
	fprintf( stderr, "cnOperate: end\n" );
#endif
	return ((*active)||(*new));
}

