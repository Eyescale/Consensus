#include <stdio.h>
#include <stdlib.h>

#include "traverse.h"
#include "cell.h"
#include "operation.h"

// #define DEBUG

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

void
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
//	cellOperate
//===========================================================================
static void enlist( Registry *subs, Registry *index, Registry *warden );
static void free_CB( Registry *, Pair *entry );

int
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
	freeRegistry( warden, free_CB );
	freeRegistry( index[ 0 ], NULL );
	freeRegistry( index[ 1 ], NULL );
#ifdef DEBUG
	fprintf( stderr, "bm_operate: end\n" );
#endif
	return 1;
}
static void
enlist( Registry *index, Registry *subs, Registry *warden )
/*
	enlist subs:{[ narrative, instances:{} ]} into index of same type,
	under warden supervision: we rely on registryRegister() to return
	the found entry if there is one, and on addIfNotThere() to return
	the enlisted item if there was none, and NULL otherwise.
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
					under( index, narrative, instances )
					addItem( instances, instance ); } } }
		else {
			registryRegister( warden, narrative, sub->value );
			under( index, narrative, instances )
			for ( listItem *i=sub->value; i!=NULL; i=i->next )
				addItem( instances, i->ptr ); }
		freePair( sub ); }
}
static void
free_CB( Registry *warden, Pair *entry )
{
	freeListItem((listItem **) &entry->value );
}

