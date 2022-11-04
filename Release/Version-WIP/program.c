#include <stdio.h>
#include <stdlib.h>

#include "traverse.h"
#include "program.h"
#include "operation.h"

// #define DEBUG

//===========================================================================
//	newProgram / freeProgram
//===========================================================================
static void releaseCell( CNCell *cell );

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
//	newCell / releaseCell
//===========================================================================
CNCell *
newCell( Pair *entry, CNEntity *parent )
{
	CNCell *cell = cn_new( NULL, NULL );
	cell->sub[ 0 ] = (CNEntity *) newPair( entry, NULL );
	cell->sub[ 1 ] = (CNEntity *) newContext( cell, parent );
	return cell;
}
static void
releaseCell( CNCell *cell )
{
	if ( !cell ) return;

	// free cell's nucleus
	freePair((Pair *) cell->sub[ 0 ] );

	/* prune CNDB proxies & release cell's input connections (cell,.)
	   this includes parent connection & proxy if these were set
	*/
	CNEntity *connection;
	listItem **connections = &cell->as_sub[ 0 ];
	while (( connection = popListItem( connections ) )) {
		cn_prune( connection->as_sub[0]->ptr ); // remove proxy
		cn_release( connection ); }

	// free cell's context
	freeContext((BMContext *) cell->sub[ 1 ] );

	/* cn_release will nullify cell in subscribers' connections (.,cell)
	   subscriber is responsible for removing dangling connections -
	   see bm_context_update()
	*/
	cell->sub[ 0 ] = NULL;
	cell->sub[ 1 ] = NULL;
	cn_release( cell );
}

//===========================================================================
//	cnUpdate
//===========================================================================
#define cellUpdate( cell ) bm_context_update( cell, BMCellContext(cell) )
#define cellInit( cell ) bm_context_init( BMCellContext(cell) )

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
			cellInit( cell );
			addItem( active, cell ); }
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
	BMContext *ctx = BMCellContext( cell );
	CNDB *db = BMContextDB( ctx );
	if ( db_out(db) ) return 0;

	Registry *warden, *index[ 2 ], *swap;
	warden = newRegistry( IndexedByAddress );
	index[ 0 ] = newRegistry( IndexedByAddress );
	index[ 1 ] = newRegistry( IndexedByAddress );

	listItem *narratives = BMCellEntry( cell )->value;
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
	enlist subs:{[ narrative, instances:{} ]} into index of same type,
	under warden supervision: we rely on registryRegister() to return
	the found entry if there is one, and on addIfNotThere() to return
	the enlisted item if there was none.
*/
{
#define under( index, narrative, instances ) \
	if (!( instances )) { \
		Pair *entry = registryRegister( index, narrative, NULL ); \
		instances = (listItem **) &entry->value; }

	Pair *sub, *found;
	CNInstance *instance;
	listItem **narratives = (listItem **) &subs->entries;
	while (( sub = popListItem(narratives) )) {
		CNNarrative *narrative = sub->name;
		listItem **instances = NULL;
 		if (( found = registryLookup( warden, narrative ) )) {
			listItem **enlisted = (listItem **) &found->value;
			listItem **candidates = (listItem **) &sub->value;
			while (( instance = popListItem(candidates) )) {
				if (( addIfNotThere( enlisted, instance ) )) {
					under( index, narrative, instances );
					addItem( instances, instance ); } } }
		else {
			registryRegister( warden, narrative, sub->value );
			under( index, narrative, instances );
			for ( listItem *i=sub->value; i!=NULL; i=i->next )
				addItem( instances, i->ptr ); }
		freePair( sub ); }
}

