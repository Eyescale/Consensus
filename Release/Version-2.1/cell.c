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
newCell( Pair *entry )
{
	CNCell *cell = cn_new( NULL, NULL );
	cell->sub[ 0 ] = (CNEntity *) newPair( entry, NULL );
	cell->sub[ 1 ] = (CNEntity *) newContext( cell );
	return cell;
}

void
releaseCell( CNCell *cell )
/*
	. prune CNDB proxies & release cell's input connections (cell,.)
	  this includes Parent connection & associated proxy if set
	. nullify cell in subscribers' connections (.,cell)
	  removing cell's Self proxy ((NULL,cell),NULL) on the way
	. free cell itself - nucleus and ctx
	Assumptions:
	. connection->as_sub[ 0 ]==singleton=={ proxy }
	. connection->as_sub[ 1 ]==NULL
	. subscriber is responsible for handling dangling connections
	  see bm_context_update()
*/
{
	if ( !cell ) return;

	CNEntity *connection;
	listItem **connections = &cell->as_sub[ 0 ];
	while (( connection = popListItem( connections ) )) {
		cn_prune( connection->as_sub[0]->ptr ); // remove proxy
		CNEntity *that = connection->sub[ 1 ];
		if (( that )) removeItem( &that->as_sub[1], connection );
		cn_free( connection ); }

	connections = &cell->as_sub[ 1 ];
	while (( connection = popListItem( connections ) )) {
		if (( connection->sub[ 0 ] ))
			connection->sub[ 1 ] = NULL; 
		else {	// remove cell's Self proxy
			cn_prune( connection->as_sub[0]->ptr );
			cn_free( connection ); } }

	freePair((Pair *) cell->sub[ 0 ] );
	freeContext((BMContext *) cell->sub[ 1 ] );
	cn_free( cell );
}

//===========================================================================
//	bm_cell_operate
//===========================================================================
static void enlist( Registry *subs, Registry *index, Registry *warden );
static void free_CB( Registry *, Pair *entry );

int
bm_cell_operate( CNCell *cell, listItem **new, CNStory *story )
{
#ifdef DEBUG
	fprintf( stderr, "bm_cell_operate: bgn\n" );
#endif
	BMContext *ctx = BMCellContext( cell );
	CNDB *db = BMContextDB( ctx );
	if ( DBExitOn(db) ) return 0;

	Registry *warden, *index[ 2 ], *swap;
	warden = newRegistry( IndexedByAddress );
	index[ 0 ] = newRegistry( IndexedByAddress );
	index[ 1 ] = newRegistry( IndexedByAddress );

	listItem *narratives = BMCellEntry( cell )->value;
	CNNarrative *base = narratives->ptr; // base narrative
	registryRegister( index[ 0 ], base, newItem( NULL ) );
	do {
		listItem **active = &index[ 0 ]->entries; // narratives to be operated
		for ( Pair *entry;( entry = popListItem(active) ); freePair(entry) ) {
			CNNarrative *narrative = entry->name;
			for ( listItem **instances = (listItem **) &entry->value; (*instances); ) {
				CNInstance *instance = popListItem( instances );
				Registry *subs = newRegistry( IndexedByAddress );
				bm_operate( narrative, instance, ctx, subs, narratives, story );
				enlist( index[ 1 ], subs, warden );
				freeRegistry( subs, NULL ); } }
		swap = index[ 0 ];
		index[ 0 ] = index[ 1 ];
		index[ 1 ] = swap;
	} while (( index[ 0 ]->entries ));
	freeRegistry( warden, free_CB );
	freeRegistry( index[ 0 ], NULL );
	freeRegistry( index[ 1 ], NULL );
#ifdef DEBUG
	fprintf( stderr, "bm_cell_operate: end\n" );
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
#define Under( index, narrative, instances ) \
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
					Under( index, narrative, instances )
					addItem( instances, instance ); } } }
		else {
			registryRegister( warden, narrative, sub->value );
			Under( index, narrative, instances )
			for ( listItem *i=sub->value; i!=NULL; i=i->next )
				addItem( instances, i->ptr ); }
		freePair( sub ); }
}
static void
free_CB( Registry *warden, Pair *entry )
{
	freeListItem((listItem **) &entry->value );
}

//===========================================================================
//	bm_cell_carry
//===========================================================================
CNInstance *
bm_cell_carry( CNCell *cell, CNCell *child, int connect )
/*
	invoked by conceive() - cf. instantiate.c
*/
{
	CNInstance *proxy = NULL;
	if ( connect ) {
		// create proxy and activate connection from child to parent cell
		BMContext *ctx = BMCellContext( child );
		proxy = db_proxy( child, cell, BMContextDB(ctx) );
		ActiveRV *active = BMContextActive( ctx );
		addIfNotThere( &active->buffer->activated, proxy );

		// assign child's parent RV
		Pair *id = BMContextId( ctx );
		id->value = proxy;

		// create proxy and activate connection from parent cell to child
		ctx = BMCellContext( cell );
		proxy = db_proxy( cell, child, BMContextDB(ctx) );
		active = BMContextActive( ctx );
		addIfNotThere( &active->buffer->activated, proxy ); }

	addItem( BMCellCarry(cell), child );
	return proxy;
}

