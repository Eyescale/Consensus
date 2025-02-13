#include <stdio.h>
#include <stdlib.h>

#include "program.h"
#include "parser.h"
#include "narrative.h"
#include "instantiate.h"
#include "errout.h"

// #define DEBUG

//===========================================================================
//	cnInit / cnExit
//===========================================================================
ProgramData *
cnInit( int *argc, char ***argv ) {
	ProgramData *data = (ProgramData *) newPair( NULL, NULL );
	db_arena_init();
	int i, narg = *argc;
	char **arg = *argv;
	for ( i=1; i<narg && !strncmp(arg[i],"-D",2); i++ )
		addItem( &data->flags, arg[i]+2 );
	i--; *argc-=i; *argv+=i;
	return data; }

void
cnExit( int report, ProgramData *data )  {
	if (( data )) {
		freeListItem( &data->flags );
		freePair((Pair *) data ); }
	db_arena_exit();
	if ( report && CNMemoryUsed )
		errout( BMMemoryLeak ); }

void
cnProgramSetInteractive( ProgramData *data ) {
	if ( !data->ui.this ) data->ui.on = 1; }

//===========================================================================
//	newProgram / freeProgram
//===========================================================================
static int bm_load( BMContext *ctx, char *path, listItem *flags );

CNProgram *
newProgram( char *inipath, CNStory **story, ProgramData *data ) {
	if ( !*story ) {
		if ( !data->ui.on ) return NULL;
		*story = newStory(); }
	else if ( !registryLookup( *story, "" ) ) {
		if ( !data->ui.on ) return NULL;
		CNNarrative *base = newNarrative();
		registryRegister( *story, "", newItem(base) ); }
	Pair *entry = CNStoryMain( *story );
	if ( !entry ) {
		errout( ProgramStory );
		return NULL; }
	CNCell *cell = newCell( entry );
	if (( inipath )) {
		int errnum = bm_load( BMCellContext(cell), inipath, data->flags );
		if ( errnum ) {
			freeCell( cell );
			errout( CellLoad, inipath );
			return NULL; } }
	if ( !cell ) return NULL;
	Pair *threads = newPair( NULL, NULL );
	CNProgram *program = (CNProgram *) newPair( *story, threads );
	// program->threads->new is a list of lists
	program->threads->new = newItem( newItem( cell ) );
	if ( data->ui.on ) data->ui.this = cell;
	return program; }

void
freeProgram( CNProgram *program, ProgramData *data ) {
	if ( !program ) return;
	CNCell *cell;
	listItem **active = &program->threads->active;
	listItem **new = &program->threads->new;
	while (( cell = popListItem(active) ))
		freeCell( cell );
	while (( cell = popListItem(new) ))
		freeCell( cell );
	freePair((Pair *) program->threads );
	freePair((Pair *) program ); }

//---------------------------------------------------------------------------
//	bm_load
//---------------------------------------------------------------------------
static BMParseCB load_CB;

static int
bm_load( BMContext *ctx, char *path, listItem *flags ) {
	FILE *stream = fopen( path, "r" );
	if ( !stream ) return( errout( ContextLoad, path ), -1 );
	CNIO io;
	io_init( &io, stream, path, IOStreamFile, flags );

	BMParseData data;
	memset( &data, 0, sizeof(BMParseData) );
        data.io = &io;
	data.ctx = ctx;
	bm_parse_init( &data, BM_LOAD );
	//-----------------------------------------------------------------
	int event = 0;
	do {	event = io_read( &io, event );
		if ( io.errnum ) data.errnum = io_report( &io );
		else data.state = bm_parse_load( event, BM_LOAD, &data, load_CB );
		} while ( strcmp( data.state, "" ) && !data.errnum );
	//-----------------------------------------------------------------
	bm_parse_exit( &data );
	io_exit( &io );

	fclose( stream );
	return data.errnum; }

static int
load_CB( BMParseOp op, BMParseMode mode, void *user_data ) {
	BMParseData *data = user_data;
	if ( op==ExpressionTake ) {
		char *expression = StringFinish( data->string, 0 );
		bm_instantiate( expression, data->ctx, NULL );
		StringReset( data->string, CNStringAll ); }
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
int
cnOperate( CNProgram *program, ProgramData *data ) {
#ifdef DEBUG
	fprintf( stderr, "cnOperate: bgn\n" );
#endif
	if ( !program ) return 0;
	// user interaction
	CNStory *story = program->story;
	CNCell *cell = data->ui.this;
	if (( cell )) {
		if ( !bm_cell_out( cell ) ) {
			int done = bm_cell_input( cell, story );
			if ( done==2 ) return 0; }
		else data->ui.this = NULL; }
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
		freeCell( cell) ;
#ifdef DEBUG
	fprintf( stderr, "cnOperate: end\n" );
#endif
	return ((*active)||(*new)); }

//===========================================================================
//	cnPrintout
//===========================================================================
static BMParseCB output_CB;

int
cnPrintOut( FILE *stream, char *path, ProgramData *pdata ) {
	BMParseData data;
	memset( &data, 0, sizeof(BMParseData) );
	data.printout.stream = stream;
	data.printout.level = 1;
	CNIO io;
        data.io = &io;
	stream = fopen( path, "r" );
	if ( !stream ) return errout( ProgramPrintout, path );
	io_init( &io, stream, path, IOStreamFile, pdata->flags );
	bm_parse_init( &data, BM_LOAD );
	//-----------------------------------------------------------------
	int event = 0;
	do {	event = io_read( &io, event );
		if ( io.errnum ) data.errnum = io_report( &io );
		else data.state = bm_parse_load( event, BM_LOAD, &data, output_CB );
		} while ( strcmp( data.state, "" ) && !data.errnum );
	//-----------------------------------------------------------------
	bm_parse_exit( &data );
	io_exit( &io );
	fclose( stream );
	return data.errnum; }

static int
output_CB( BMParseOp op, BMParseMode mode, void *user_data ) {
	BMParseData *data = user_data;
	FILE *stream = data->printout.stream;
	int level = data->printout.level;
	if ( op==ExpressionTake ) {
		TAB( level );
		char *expression = StringFinish( data->string, 0 );
		fprint_expr( stream, expression, level, DO );
		StringReset( data->string, CNStringAll );
		fprintf( stream, "\n" ); }
	return 1; }

