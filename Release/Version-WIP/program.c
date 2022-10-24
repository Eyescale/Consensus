#include <stdio.h>
#include <stdlib.h>

#include "program.h"
#include "operation.h"

// #define DEBUG

//===========================================================================
//	newProgram / freeProgram
//===========================================================================
CNProgram *
newProgram( CNStory *story, char *inipath )
{
	if ( story == NULL ) return NULL;
	Pair *entry = registryLookup( story, "" );
	if ( !entry || entry->value == NULL ) {
		fprintf( stderr, "B%%: Error: story has no main\n" );
		return NULL; }
	CNDB *db = newCNDB();
	CNCell *cell = newCell( entry, db, NULL );
	if ( !cell ) {
		fprintf( stderr, "B%%: Error: unable to allocate Cell\n" );
		goto ERR; }
	if (( inipath )) {
		if ( !!bm_read( BM_LOAD, cell->ctx, inipath ) ) {
			fprintf( stderr, "B%%: Error: load init file: '%s' failed\n", inipath );
			goto ERR; } }
	// threads->new is a list of lists
	Pair *threads = newPair( NULL, newItem(newItem(cell)) );
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
//	cnUpdate
//===========================================================================
void
cnUpdate( CNProgram *program )
{
#ifdef DEBUG
	fprintf( stderr, "cnUpdate: bgn\n" );
#endif
	if ( program == NULL ) return;

	listItem **active = &program->threads->active;
	listItem **new = &program->threads->new;
	// update active cells
	for ( listItem *i=*active; i!=NULL; i=i->next ) {
		CNCell *cell = i->ptr;
		BMContext *ctx = cell->ctx;
		bm_update( ctx );
		addItem( new, *BMContextCarry(ctx) ); }

	// activate new cells
	for ( listItem *i; (i=popListItem(new)); )
		for ( listItem *j=i; j!=NULL; j=j->next ) {
			CNCell *cell = j->ptr;
			BMContext *ctx = cell->ctx;
			// integrate init conditions
			bm_update( ctx );
			// re-init cell for its own narrative
			db_init( BMContextDB(ctx) );
			addItem( active, cell ); }
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
		} }
	// release deactivated cells
	while (( cell = popListItem(&released) ))
		freeCell( cell );
#ifdef DEBUG
	fprintf( stderr, "cnOperate: end\n" );
#endif
	return ((*active)||(*new));
}

//===========================================================================
//	newCell / freeCell
//===========================================================================
CNCell *
newCell( Pair *entry, CNDB *db, CNEntity *parent )
{
	if ( !db ) return NULL;
	BMContext *ctx = newContext( db, parent );
	if ( !ctx ) return NULL;
	return (CNCell *) newPair( entry, ctx );
}
void
freeCell( CNCell *cell )
{
	freeContext( cell->ctx );
	freePair((Pair *) cell );
}

