#include <stdio.h>
#include <stdlib.h>

#include "program.h"
#include "operation.h"

// #define DEBUG

//===========================================================================
//	newProgram / freeProgram
//===========================================================================
static CNCell *newCell( Pair *, CNDB * );
void freeCell( CNCell * );

CNProgram *
newProgram( CNStory *story, char *inipath )
{
	if ( story == NULL ) return NULL;

	Pair *entry = registryLookup( story, "" );
	if ( !entry || entry->value == NULL ) {
		fprintf( stderr, "B%%: Error: story has no main\n" );
		return NULL;
	}
	CNDB *db = newCNDB();
	CNCell *cell = newCell( entry, db );
	if ( !cell ) {
		fprintf( stderr, "B%%: Error: unable to allocate Cell\n" );
		goto ERR;
	}
	if (( inipath )) {
		if ( !!bm_read( CN_INI, cell->ctx, inipath ) ) {
			fprintf( stderr, "B%%: Error: load init file: '%s' failed\n", inipath );
			goto ERR;
		}
	}
	Pair *threads = newPair( NULL, newItem(cell) );
	return (CNProgram *) newPair( story, threads );
ERR:
	freeCNDB( db );
	return NULL;
}

void
freeProgram( CNProgram *program )
{
	if ( program == NULL ) return;

	CNCell *cell;
	listItem **active = &program->threads->active;
	listItem **new = &program->threads->new;
	while (( cell = popListItem(active) ))
		freeCell( cell );
	while (( cell = popListItem(new) ))
		freeCell( cell );
	freePair((Pair *) program->threads );
	freePair((Pair *) program );
}

//===========================================================================
//	newCell / freeCell
//===========================================================================
CNCell *
newCell( Pair *entry, CNDB *db )
{
	if ( !db ) return NULL;
	BMContext *ctx = newContext( db );
	if ( !ctx ) return NULL;
	return (CNCell *) newPair( entry, ctx );
}
void
freeCell( CNCell *cell )
{
	freeContext( cell->ctx );
	freePair((Pair *) cell );
}

//===========================================================================
//	cnUpdate
//===========================================================================
void
cnUpdate( CNProgram *program )
{
	if ( program == NULL ) return;

	CNCell *cell;
	listItem **active = &program->threads->active;
	listItem **new = &program->threads->new;
	// update active cells
	for ( listItem *i=*active; i!=NULL; i=i->next ) {
		cell = i->ptr;
		bm_update( cell->ctx, 0 );
	}
	// activate new cells
	while (( cell = popListItem(new) )) {
		bm_update( cell->ctx, 1 );
		addItem( active, cell );
	}
}

//===========================================================================
//	cnOperate
//===========================================================================
int
cnOperate( CNProgram *program )
{
	if ( program == NULL ) return 0;

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
		if ( bm_operate( cell, new, story ) )
			last_i = i;
		else {
			addItem( &released, cell );
			clipListItem( active, i, last_i, next_i );
		}
	}
	// release deactivated cells
	while (( cell = popListItem(&released) ))
		freeCell( cell );
	return ((*active)||(*new));
}

