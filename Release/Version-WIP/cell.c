#include <stdio.h>
#include <stdlib.h>

#include "cell.h"
#include "operation.h"
#include "parser.h"
#include "errout.h"

// #define DEBUG

//===========================================================================
//	newCell / freeCell
//===========================================================================
CNCell *
newCell( Pair *narrative ) {
	CNEntity *this = cn_new( NULL, NULL );
	this->sub[ 0 ] = (CNEntity *) newPair( narrative, NULL );
	this->sub[ 1 ] = (CNEntity *) newContext( this );
	return this; }

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
*/ {
	if ( !cell ) return;
	listItem **connections;
	CNEntity *connection, *that;

	connections = &cell->as_sub[ 0 ];
	while (( connection=popListItem( connections ) )) {
		if (( that=connection->sub[ 1 ] ))
			removeItem( &that->as_sub[1], connection );
		cn_prune( connection->as_sub[0]->ptr );
		cn_free( connection ); } // remove proxy

	connections = &cell->as_sub[ 1 ];
	while (( connection=popListItem( connections ) )) {
		if (( connection->sub[ 0 ] )) {
			connection->sub[ 1 ] = NULL; 
			continue; }
		cn_prune( connection->as_sub[0]->ptr );
		cn_free( connection ); } // remove self

	freePair((Pair *) cell->sub[ 0 ] );
	freeContext((BMContext *) cell->sub[ 1 ] );
	cn_free( cell ); }

//===========================================================================
//	bm_bond
//===========================================================================
void
bm_bond( CNEntity *this, CNEntity *child, CNInstance *proxy ) {
	BMContext *ctx;

	// activate connection from parent to child
	ctx = BMCellContext( this );
	ActiveRV *active = BMContextActive( ctx );
	addIfNotThere( &active->buffer->activated, proxy );

	// activate connection from child to parent
	ctx = BMCellContext( child );
	proxy = new_proxy( child, this, BMContextDB(ctx) );
	active = BMContextActive( ctx );
	addIfNotThere( &active->buffer->activated, proxy );

	// inform child's parent RV
	Pair *id = BMContextId( ctx );
	id->value = proxy; }

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
static void enlist( Registry *, Registry *, Registry *, Registry *warden[2], CNNarrative ** );
static void free_CB( Registry *warden, Pair *entry ) {
	freeListItem((listItem **) &entry->value ); }
static inline int SWAP( Registry *index[2] ) {
	if (( index[ 1 ]->entries )) {
		void *swap = index[ 0 ];
		index[ 0 ] = index[ 1 ];
		index[ 1 ] = swap;
		return 1; }
	else return 0; }
#define FOREACH( index ) \
	/* active is the list of narratives to be operated */	\
	listItem **active = &index[ 0 ]->entries;		\
	for ( Pair *entry;( entry = popListItem(active) ); freePair(entry) ) {	\
		CNNarrative *narrative = entry->name;				\
		listItem **instances = (listItem **) &entry->value;		\
		while (( *instances )) {					\
			CNInstance *current = popListItem( instances );

void
bm_cell_operate( CNCell *cell, CNStory *story ) {
#ifdef DEBUG
	fprintf( stderr, "bm_cell_operate: bgn\n" );
#endif
	Registry *enabled[2], *invoked[2], *warden[2], *subs;
	warden[ 0 ] = newRegistry( IndexedByAddress );
	enabled[ 0 ] = newRegistry( IndexedByAddress );
	enabled[ 1 ] = newRegistry( IndexedByAddress );
	invoked[ 0 ] = newRegistry( IndexedByAddress );
	invoked[ 1 ] = newRegistry( IndexedByAddress );
	subs = newRegistry( IndexedByAddress );

	CNNarrative *pf = NULL;
	BMContext *ctx = BMCellContext( cell );
	listItem *narratives = BMCellEntry( cell )->value;
	CNNarrative *base = narratives->ptr; // base narrative
	registryRegister( enabled[ 1 ], base, newItem( NULL ) );
	while ( SWAP( enabled ) ) {
		warden[ 1 ] = newRegistry( IndexedByAddress );
		FOREACH( enabled ) // { {
			bm_context_actualize( ctx, narrative->proto, current );
			bm_operate( narrative, ctx, story, narratives, subs );
			enlist( subs, enabled[1], invoked[1], warden, &pf );
			CNInstance *parent = current;
			while ( SWAP( invoked ) ) {
				FOREACH( invoked ) // { {
					bm_context_push( ctx, ".", parent );
					bm_context_actualize( ctx, narrative->proto, current );
					bm_operate( narrative, ctx, story, narratives, subs );
					enlist( subs, enabled[1], invoked[1], warden, &pf );
					bm_context_pop( ctx, "." ); } } }
			bm_context_release( ctx ); }
		freeRegistry( warden[ 1 ], free_CB ); } }

	if (( pf )) { // execute post-frame narrative
		bm_context_actualize( ctx, pf->proto, NULL );
		bm_operate( pf, ctx, story, narratives, NULL );
		bm_context_release( ctx ); }

	freeRegistry( warden[ 0 ], free_CB );
	freeRegistry( enabled[ 0 ], NULL );
	freeRegistry( invoked[ 0 ], NULL );
	freeRegistry( enabled[ 1 ], NULL );
	freeRegistry( invoked[ 1 ], NULL );
	freeRegistry( subs, NULL );
#ifdef DEBUG
	fprintf( stderr, "bm_cell_operate: end\n" );
#endif
	}

#define FETCH( ndx, narrative, instances ) if ( !instances ) { \
	Registry *index = ndx ? invoke : enable; \
	Pair *entry = registryRegister( index, narrative, NULL ); \
	instances = (listItem **) &entry->value; }

static void
enlist( Registry *subs, Registry *enable, Registry *invoke, Registry *warden[2], CNNarrative **pf )
/*
	enlist subs:{[ narrative, instances:{} ]} into index of same type,
	under warden supervision: we rely on registryRegister() to return
	the found entry if there is one, and on addIfNotThere() to return
	the enlisted item if there was none, and NULL otherwise.
*/ {
	Pair *sub, *found;
	CNInstance *instance;
	listItem **candidates = (listItem **) &subs->entries;
	while (( sub = popListItem(candidates) )) {
		CNNarrative *narrative = sub->name;
		if ( !strcmp( ".:&", narrative->proto ) ) {
			// assumption here: sub->value==NULL
			if ( !*pf ) *pf = narrative;
			freePair( sub );
			continue; }
		listItem **instances = NULL;
		int ndx = narrative->proto[0]=='.' ? 0 : 1;
 		if (( found = registryLookup( warden[ ndx ], narrative ) )) {
			listItem **enlisted = (listItem **) &found->value;
			listItem **candidates = (listItem **) &sub->value;
			while (( instance = popListItem(candidates) )) {
				if (( addIfNotThere( enlisted, instance ) )) {
					FETCH( ndx, narrative, instances )
					addItem( instances, instance ); } } }
		else {
			registryRegister( warden[ ndx ], narrative, sub->value );
			FETCH( ndx, narrative, instances )
			for ( listItem *i=sub->value; i!=NULL; i=i->next )
				addItem( instances, i->ptr ); }
		freePair( sub ); } }

