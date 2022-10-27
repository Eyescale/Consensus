#include <stdio.h>
#include <stdlib.h>

#include "program.h"
#include "operation.h"

// #define DEBUG

//===========================================================================
//	newProgram / freeProgram
//===========================================================================
static void freeCell( CNCell *cell );

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
	if (( inipath && !!bm_read( BM_LOAD, cell->ctx, inipath ) )) {
		fprintf( stderr, "B%%: Error: load init file: '%s' failed\n", inipath );
		freeCell( cell ); return NULL; }
	// threads->new is a list of lists
	Pair *threads = newPair( NULL, newItem(newItem(cell)) );
	return (CNProgram *) newPair( story, threads );
}

void
freeProgram( CNProgram *program )
{
	if ( !program ) return;
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
	if ( !program ) return;
	listItem **active = &program->threads->active;
	listItem **new = &program->threads->new;
	// update active cells
	for ( listItem *i=*active; i!=NULL; i=i->next ) {
		CNCell *cell = i->ptr;
		BMContext *ctx = cell->ctx;
		bm_context_update( ctx );
		listItem *carry = *BMContextCarry( ctx );
		if (( carry )) addItem( new, carry ); }
	// activate new cells
	for ( listItem *i; (i=popListItem(new)); )
		for ( listItem *j=i; j!=NULL; j=j->next ) {
			CNCell *cell = j->ptr;
			bm_context_init( cell->ctx );
			addItem( active, cell ); }
#ifdef DEBUG
	fprintf( stderr, "cnUpdate: end\n" );
#endif
}

//===========================================================================
//	cnOperate
//===========================================================================
static int cellOperate( CNCell *cell, listItem **new, CNStory *story );

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
		if ( cellOperate( cell, new, story ) )
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
newCell( Pair *entry, CNEntity *parent )
{
	BMContext *ctx = newContext( parent );
	return (ctx) ? (CNCell *) newPair( entry, ctx ) : NULL;
}
static void
freeCell( CNCell *cell )
{
	if ( !cell ) return;
	freeContext( cell->ctx );
	freePair((Pair *) cell );
}

//===========================================================================
//	cellOperate
//===========================================================================
static void enlist( Registry *subs, Registry *index, Registry *warden );
static void relieve_CB( Registry *, Pair *entry );

static int
cellOperate( CNCell *cell, listItem **new, CNStory *story )
{
#ifdef DEBUG
	fprintf( stderr, "bm_operate: bgn\n" );
#endif
	BMContext *ctx = cell->ctx;
	freeListItem( BMContextCarry(ctx) );
	CNDB *db = BMContextDB( ctx );
	if ( db_out(db) ) return 0;

	Registry *warden, *index[ 2 ], *swap;
	warden = newRegistry( IndexedByAddress );
	index[ 0 ] = newRegistry( IndexedByAddress );
	index[ 1 ] = newRegistry( IndexedByAddress );

	listItem *narratives = cell->entry->value;
	CNNarrative *base = narratives->ptr; // base narrative
	registryRegister( index[ 1 ], base, newItem( NULL ) );
	do {
		swap = index[ 0 ];
		index[ 0 ] = index[ 1 ];
		index[ 1 ] = swap;
		listItem **active = &index[ 0 ]->entries; // narratives to be operated
		for ( Pair *entry;( entry = popListItem(active) ); freePair(entry) ) {
			CNNarrative *narrative = entry->name;
			for ( listItem **instances = (listItem **) &entry->value; (*instances); ) {
				CNInstance *instance = popListItem( instances );
				Registry *subs = newRegistry( IndexedByAddress );
				bm_operate( narrative, ctx, instance, subs, narratives, story );
				enlist( index[ 1 ], subs, warden );
				freeRegistry( subs, NULL ); } }
	} while (( index[ 1 ]->entries ));
	freeRegistry( warden, relieve_CB );
	freeRegistry( index[ 0 ], NULL );
	freeRegistry( index[ 1 ], NULL );
#ifdef DEBUG
	fprintf( stderr, "bm_operate: end\n" );
#endif
	return 1;
}
static void
relieve_CB( Registry *warden, Pair *entry )
{
	freeListItem((listItem **) &entry->value );
}
static void
enlist( Registry *index, Registry *subs, Registry *warden )
/*
	enlist subs:%{[ narrative, instance:%{} ]} into index of same type,
	under warden supervision: we rely on registryRegister() to return
	the found entry if there is one, and on addIfNotThere() to return
	NULL is there is none.
*/
{
#define s_place( s, index, narrative ) \
	if (!( s )) { \
		Pair *entry = registryRegister( index, narrative, NULL ); \
		s = (listItem **) &entry->value; }

	listItem **narratives = (listItem **) &subs->entries;
	for ( Pair *sub;( sub = popListItem(narratives) ); freePair(sub) ) {
		CNNarrative *narrative = sub->name;
		listItem **selection = NULL;
 		Pair *found = registryLookup( warden, narrative );
		if (!( found )) {
			registryRegister( warden, narrative, sub->value );
			s_place( selection, index, narrative );
			for ( listItem *i=sub->value; i!=NULL; i=i->next )
				addItem( selection, i->ptr ); }
		else {
			listItem **enlisted = (listItem **) &found->value;
			listItem **candidates = (listItem **) &sub->value;
			for ( CNInstance *instance;( instance = popListItem(candidates) ); ) {
				if (( addIfNotThere( enlisted, instance ) )) {
					s_place( selection, index, narrative );
					addItem( selection, instance );
		} } } }
}

