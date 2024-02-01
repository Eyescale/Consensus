#include <stdio.h>
#include <stdlib.h>

#include "program.h"
#include "parser.h"
#include "expression.h"

// #define DEBUG

//===========================================================================
//	cnIniOutput
//===========================================================================
static BMParseCB output_CB;

int
cnIniOutput( FILE *output_stream, char *path, int level ) {
	FILE *stream = fopen( path, "r" );
	if ( !stream ) {
		fprintf( stderr, "B%%: Error: no such file or directory: '%s'\n", path );
		return -1; }
	CNIO io;
	io_init( &io, stream, path, IOStreamFile );
	BMParseData data;
	memset( &data, 0, sizeof(BMParseData) );
	data.entry = newPair( output_stream, cast_ptr( level ) );
        data.io = &io;
	bm_parse_init( &data, BM_LOAD );
	//-----------------------------------------------------------------
	int event = 0;
	do {	event = io_read( &io, event );
		if ( io.errnum ) data.errnum = io_report( &io );
		else data.state = bm_parse_load( event, BM_LOAD, &data, output_CB );
		} while ( strcmp( data.state, "" ) && !data.errnum );
	//-----------------------------------------------------------------
	freePair( data.entry );
	bm_parse_exit( &data );
	io_exit( &io );
	fclose( stream );
	return data.errnum; }

static int
output_CB( BMParseOp op, BMParseMode mode, void *user_data ) {
	BMParseData *data = user_data;
	FILE *stream = data->entry->name;
	int level = cast_i( data->entry->value );
	if ( op==ExpressionTake ) {
		TAB( level );
		char *expression = StringFinish( data->string, 0 );
		fprint_expr( stream, expression, level );
		StringReset( data->string, CNStringAll );
		fprintf( stream, "\n" ); }
	return 1; }

//===========================================================================
//	cnSync
//===========================================================================
void
cnSync( CNProgram *program ) {
#ifdef DEBUG
	fprintf( stderr, "cnSync: bgn\n" );
#endif
	if ( !program ) return;
	CNStory *story = program->story;
	CNCell *cell;
	// update active cells
	listItem *out = NULL;
	listItem **active = &program->threads->active;
	for ( listItem *i=*active; i!=NULL; i=i->next ) {
		cell = i->ptr;
		if ( bm_cell_update( cell, story ) )
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
static inline void uncache( listItem *flushed, listItem *narratives );
static freeCellCB arena_flush_CB;

int
cnOperate( CNCell **this, CNProgram *program ) {
#ifdef DEBUG
	fprintf( stderr, "cnOperate: bgn\n" );
#endif
	if ( !program ) return 0;
	// user interaction
	CNStory *story = program->story;
	CNCell *cell = ((this) ? *this : NULL );
	if (( cell )) {
		int done = bm_cell_read( this, story );
		if ( done==2 ) return 0; }
	// operate active cells
	listItem **active = &program->threads->active;
	listItem **new = &program->threads->new;
	listItem *carry, *out=NULL;
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
	// free out cells
	while (( cell = popListItem(&out) ))
		freeCell( cell, arena_flush_CB, program->story );
#ifdef DEBUG
	fprintf( stderr, "cnOperate: end\n" );
#endif
	return ((*active)||(*new)); }

static void
arena_flush_CB( BMContext *ctx, void *user_data ) {
	listItem *flushed = NULL;
	CNStory *story = user_data;
	CNDB *db = BMContextDB( ctx );
	Pair *shared = registryLookup( ctx, "$" )->value;
	bm_arena_flush( story->arena, shared, db, &flushed );
	if (( flushed )) {
		uncache( flushed, story->narratives->entries );
		freeListItem( &flushed ); } }

static inline void
uncache( listItem *flushed, listItem *narratives ) {
	for ( listItem *i=narratives; i!=NULL; i=i->next ) {
		Pair *entry = i->ptr;
		narratives = entry->value;
		for ( listItem *j=narratives->next; j!=NULL; j=j->next ) {
			CNNarrative *narrative = j->ptr;
			void *shared = narrative->root->data->expression;
			if (( shared )&&( lookupIfThere(flushed,shared) ))
				narrative->root->data->expression = NULL; } } }

//===========================================================================
//	newProgram / freeProgram
//===========================================================================
CNProgram *
newProgram( CNStory *story, char *inipath ) {
	Pair *entry = CNStoryMain( story );
	if ( !entry ) {
		fprintf( stderr, "B%%: Error: story has no main\n" );
		return NULL; }
	CNCell *cell = newCell( entry, inipath );
	if ( !cell ) return NULL;
	CNProgram *program = (CNProgram *) newPair( story, newPair(NULL,NULL) );
	// threads->new is a list of lists
	program->threads->new = newItem( newItem( cell ) );
	return program; }

void
freeProgram( CNProgram *program ) {
	if ( !program ) return;
	CNCell *cell;
	listItem **active = &program->threads->active;
	listItem **new = &program->threads->new;
	while (( cell = popListItem(active) )) {
		freeCell( cell, arena_flush_CB, program->story ); }
	while (( cell = popListItem(new) )) {
		freeCell( cell, arena_flush_CB, program->story ); }
	freePair((Pair *) program->threads );
	freePair((Pair *) program ); }

