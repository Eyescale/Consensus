#include <stdio.h>
#include <stdlib.h>

#include "traverse.h"
#include "cell.h"
#include "operation.h"
#include "parser.h"

// #define DEBUG

//===========================================================================
//	bm_cell_read
//===========================================================================
int
bm_cell_read( CNCell **this, CNStory *story ) {
	if ( bm_cell_out(*this) ) { *this=NULL; return 1; }
	CNIO io;
	memset( &io, 0, sizeof(CNIO) );
	io.stream = stdin;
	BMParseData data;
	memset( &data, 0, sizeof(BMParseData) );
	data.io = &io;
	data.ctx = BMMakeCurrent( *this );
	int done = 0;
	do {	printf( "B%% " ); // prompt
		io_reset( &io, 1, 3 );
		bm_parse_init( &data, BM_CMD );
		//------------------------------------------------------------
		int event = 0;
		do {	event = io_getc( &io, event );
			data.state = data.expr ?
				bm_parse_expr( event, BM_CMD, &data, NULL ) :
				bm_parse_ui( event, BM_CMD, &data, NULL );
			} while ( strcmp(data.state,"") && !data.errnum );
		//------------------------------------------------------------
		if ( data.errnum )
			fpurge( stdin );
		else if ( event==EOF ) {
			printf( "  \n" ); // overwrite ^D
			bm_op( DO, "exit", data.ctx, story );
			done = 1; }
		else if ( data.expr ) {
			char *action = StringFinish( data.string, 0 );
			bm_op( data.type, action, data.ctx, story );
			done = ( data.type==OUTPUT ? 0 : 1 ); }
		else { // execute cmd - which if nop does forces sync
			done = ( StringCompare(data.string,"/") ? 1 : 2 ); }
		bm_parse_exit( &data );
	} while ( !done );
	return done; }

//===========================================================================
//	bm_cell_init, bm_cell_update
//===========================================================================
// see cell.h

//===========================================================================
//	bm_cell_operate
//===========================================================================
static void enlist( Registry *subs, Registry *index, Registry *warden );
static void free_CB( Registry *, Pair *entry );
static inline int SWAP( Registry *index[ 2 ] ) {
	if (( index[ 1 ]->entries )) {
		void *swap = index[ 0 ];
		index[ 0 ] = index[ 1 ];
		index[ 1 ] = swap;
		return 1; }
	else return 0; }

void
bm_cell_operate( CNCell *cell, CNStory *story ) {
#ifdef DEBUG
	fprintf( stderr, "bm_cell_operate: bgn\n" );
#endif
	Registry *warden, *index[ 2 ], *subs;
	warden = newRegistry( IndexedByAddress );
	index[ 0 ] = newRegistry( IndexedByAddress );
	index[ 1 ] = newRegistry( IndexedByAddress );
	subs = newRegistry( IndexedByAddress );

	BMContext *ctx = BMCellContext( cell );
	listItem *narratives = BMCellEntry( cell )->value;
	CNNarrative *base = narratives->ptr; // base narrative
	registryRegister( index[ 1 ], base, newItem( NULL ) );
	while ( SWAP(index) ) {
		listItem **active = &index[ 0 ]->entries; // narratives to be operated
		for ( Pair *entry;( entry = popListItem(active) ); freePair(entry) ) {
			CNNarrative *narrative = entry->name;
			listItem **instances = (listItem **) &entry->value;
			while (( *instances )) {
				CNInstance *instance = popListItem( instances );
				bm_context_actualize( ctx, narrative->proto, instance );
				bm_operate( narrative, ctx, story, narratives, subs );
				enlist( subs, index[ 1 ], warden );
				bm_context_release( ctx ); } } }
	freeRegistry( warden, free_CB );
	freeRegistry( index[ 0 ], NULL );
	freeRegistry( index[ 1 ], NULL );
	freeRegistry( subs, NULL );
#ifdef DEBUG
	fprintf( stderr, "bm_cell_operate: end\n" );
#endif
	}

static void
enlist( Registry *subs, Registry *index, Registry *warden )
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
		freePair( sub ); } }

static void
free_CB( Registry *warden, Pair *entry ) {
	freeListItem((listItem **) &entry->value ); }

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
	return proxy; }

//===========================================================================
//	newCell / freeCell
//===========================================================================
CNCell *
newCell( Pair *entry, char *inipath ) {
	CNCell *cell = cn_new( NULL, NULL );
	cell->sub[ 0 ] = (CNEntity *) newPair( entry, NULL );
	cell->sub[ 1 ] = (CNEntity *) newContext( cell );
	if (( inipath )) {
		int errnum = bm_load( inipath, BMCellContext(cell) );
		if ( errnum ) {
			fprintf( stderr, "B%%: Error: load init file: '%s' failed\n", inipath );
			freeCell( cell );
			return NULL; } }
	return cell; }

void
freeCell( CNCell *cell )
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
	cn_free( cell ); }

