#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "database.h"
#include "expression.h"
#include "instantiate.h"
#include "scour.h"
#include "story.h"
#include "eenov.h"
#include "parser.h"
#include "errout.h"

// #define DEBUG

//===========================================================================
//	bm_feel
//===========================================================================
// cf include/expression.h

//===========================================================================
//	bm_scan, bm_forescan
//===========================================================================
static BMQueryCB scan_CB;
listItem *
bm_scan( char *expression, BMContext *ctx ) {
	listItem *results = NULL;
	bm_query( BM_CONDITION, expression, ctx, scan_CB, &results );
	reorderListItem( &results );
	return results; }

static BMQTake
scan_CB( CNInstance *e, BMContext *ctx, void *user_data ) {
	listItem **results = user_data;
	addItem( results, e );
	return BMQ_CONTINUE; }

//===========================================================================
//	bm_release
//===========================================================================
static BMQueryCB release_CB;
void
bm_release( char *expression, BMContext *ctx ) {
	if ( !strcmp( expression, "(~.)" ) ) return;
	CNDB *db = BMContextDB( ctx );
	bm_query( BM_CONDITION, expression, ctx, release_CB, db ); }

static BMQTake
release_CB( CNInstance *e, BMContext *ctx, void *user_data ) {
	db_deprecate( e, (CNDB *) user_data );
	return BMQ_CONTINUE; }

//===========================================================================
//	bm_switch / bm_case
//===========================================================================
// see include/expression.h

//===========================================================================
//	bm_enable
//===========================================================================
static BMQueryCB enable_CB;

void
bm_enable( int rv, void *rvv, char *en, BMContext *ctx, EnableData *data ) {
	switch ( rv ) {
	case 0:	bm_query( BM_CONDITION, en, ctx, enable_CB, data ); break;
	case 1: enable_CB( rvv, ctx, data ); break;
	case 3: for ( listItem *i=rvv; i!=NULL; i=i->next )
			enable_CB( i->ptr, ctx, data ); } }

static BMQTake
enable_CB( CNInstance *e, BMContext *ctx, void *user_data ) {
	EnableData *data = user_data;
	Registry *subs = data->subs;
	listItem *narratives = data->narratives;
	uint *key = cnIsShared(e) ? CNSharedKey( e ) : NULL;
	for ( listItem *i=narratives->next; i!=NULL; i=i->next ) {
		CNNarrative *narrative = i->ptr;
		char *p = narrative->proto, *q;
		if ( *p==':' || !strcmp(p,".:&") ) continue;
		p = p_prune( PRUNE_IDENTIFIER, p+1 );
		p++; // skip leading ':'
		switch ( *p ) {
		case '"':
			if ( !db_arena_verify(key,p) ) continue;
			registryRegister( subs, narrative, newItem(e) );
			return BMQ_CONTINUE;
		default:
			if ( !proto_verify(p,e,ctx) ) continue;
			Pair *entry = registryLookup( subs, narrative );
			if (( entry )) addIfNotThere((listItem **) &entry->value, e );
			else registryRegister( subs, narrative, newItem(e) ); } }
	return BMQ_CONTINUE; }

//===========================================================================
//	bm_enable_x
//===========================================================================
typedef struct {
	CNNarrative *narrative;
	char *p;
	Registry *subs;
	} EnableXData;
static BMQueryCB enable_x_CB;

int
bm_enable_x( char *en, BMContext *ctx, listItem *narratives, Registry *subs ) {
	for ( listItem *i=narratives->next; i!=NULL; i=i->next ) {
		CNNarrative *narrative = i->ptr;
		char *p = narrative->proto, *q;
		if ( *p++!=':' ) continue;
		// we have p: "func(params)" and en: "func expr"
		if ( !strcomp( p, en, 1 ) ) {
			p = p_prune( PRUNE_IDENTIFIER, p );
			q = p_prune( PRUNE_IDENTIFIER, en );
			EnableXData data = { narrative, p, subs };
			int rv=( *q=='(' ); void *rvv;
			if ( rv ) rvv = BMContextRVTest( ctx, q+1, &rv );
			switch ( rv ) {
			case 0:	bm_query( BM_CONDITION, q, ctx, enable_x_CB, &data ); break;
			case 1: enable_x_CB( rvv, ctx, &data ); break;
			case 3: for ( listItem *i=rvv; i!=NULL; i=i->next )
					enable_x_CB( i->ptr, ctx, &data ); }
			return 1; } }
	return 0; }

static BMQTake
enable_x_CB( CNInstance *e, BMContext *ctx, void *user_data ) {
	EnableXData *data = user_data;
	if ( proto_verify( data->p, e, ctx ) ) {
		Registry *subs = data->subs;
		CNNarrative *narrative = data->narrative;
		Pair *entry = registryLookup( subs, narrative );
		if (( entry )) addIfNotThere((listItem **) &entry->value, e );
		else registryRegister( subs, narrative, newItem(e) ); }
	return BMQ_CONTINUE; }

//===========================================================================
//	bm_tag_traverse
//===========================================================================
int
bm_tag_traverse( char *tag, char *p, BMContext *ctx )
/*
	we have	  v---------------- tag
		.%identifier:{_}
			    v^--------------p
		.%identifier~:{_}
	Note that we do not free tag entry in context registry
*/ {
	Pair *entry = BMTag( ctx, tag );
	if ( !entry ) {
//		errout( ExpressionTagUnknown, tag );
		return 0; }
	int released = ( *p=='~' ? (p+=2,1) : 0 );

	Pair *current = Mset( ctx, NULL );
	listItem *next_i, *last_i=NULL;
	listItem **entries = (listItem **) &entry->value;
	for ( listItem *i=*entries; i!=NULL; i=next_i ) {
		next_i = i->next;
		current->value = i->ptr;
		int clip = released;
		for ( char *q=p; *q++!='}'; q=p_prune(PRUNE_LEVEL,q) )
			bm_query( BM_CONDITION, q, ctx, NULL, &clip );
		if (( clip ))
			clipListItem( entries, i, last_i, next_i );
		else last_i = i; }
	Mclr( ctx );
	return 1; }

//===========================================================================
//	bm_tag_inform
//===========================================================================
static BMQueryCB inform_CB;
int
bm_tag_inform( char *tag, char *p, BMContext *ctx )
/*
	we have	  v---------------- tag
		.%identifier:<_>
			     ^--------------p
	Note that we do reset tag values
*/ {
	Pair *entry = BMTag( ctx, tag );
	if ( !entry ) entry = bm_tag( ctx, tag, NULL );
	else freeListItem((listItem **) &entry->value );

	for ( char *q=p; *q++!='>'; q=p_prune(PRUNE_LEVEL,q) )
		bm_query( BM_CONDITION, q, ctx, inform_CB, &entry->value );
	return 1; }

static BMQTake
inform_CB( CNInstance *e, BMContext *ctx, void *user_data ) {
	addIfNotThere((listItem **) user_data, e );
	return BMQ_CONTINUE; }

//===========================================================================
//	bm_tag_set
//===========================================================================
int
bm_tag_set( char *tag, listItem *results, BMContext *ctx ) {
	Pair *entry = BMTag( ctx, tag );
	if ( !entry ) entry = bm_tag( ctx, tag, results );
	else {
		freeListItem((listItem **) &entry->value );
		entry->value = results; }
	return 1; }

//===========================================================================
//	bm_tag_clear
//===========================================================================
static freeRegistryCB clear_CB;
int
bm_tag_clear( char *tag, BMContext *ctx )
/*
	we have	  v---------------- tag
		.%identifier~
	Note that we do free tag entry in context registry
*/ {
	Pair *entry = BMTag( ctx, tag );
	if ( !entry ) {
//		errout( ExpressionTagUnknown, tag );
		return 0; }
	bm_untag( ctx, tag );
	return 1; }

//===========================================================================
//	bm_inputf
//===========================================================================
static int bm_input( char *fmt, char *arg, BMContext * );
#define DEFAULT_FMT "_"
int
bm_inputf( char *fmt, listItem *args, BMContext *ctx )
/*
	Assumption: fmt starts and finishes with \" or \0
*/ {
	int event=0, delta;
	char_s q;
	if ( *fmt ) {
		fmt++; // skip opening double-quote
		while ( *fmt ) {
			switch (*fmt) {
			case '"':
				goto RETURN;
			case '%':
				if ( !fmt[1] ) goto RETURN;
				else if ( fmt[1]=='%' ) {
					event = fgetc(stdin);
					if ( event==EOF ) goto RETURN;
					else if ( event!='%' ) {
						ungetc( event, stdin );
						goto RETURN; } }
				else if ((args)) {
					char *arg = args->ptr;
					event = bm_input( fmt+1, arg, ctx );
					if ( event==EOF ) goto RETURN;
					args = args->next; }
				fmt+=2; break;
			default:
				if (( delta=charscan( fmt, &q ) )) {
					event = fgetc( stdin );
					if ( event==EOF ) goto RETURN;
					else if ( event!=q.value ) {
						ungetc( event, stdin );
						fmt += delta; } }
				else fmt++; } } }
	else {
		while (( args )) {
			char *arg = args->ptr;
			event = bm_input( DEFAULT_FMT, arg, ctx );
			if ( event==EOF ) break;
			else args = args->next; } }
RETURN:
	if ( event==EOF ) {
		while (( args )) {
			char *arg = args->ptr;
			bm_instantiate_input( arg, NULL, ctx );
			args = args->next; } }
	return 0; }

static char * bm_read( FILE *stream );
static int
bm_input( char *fmt, char *arg, BMContext *ctx ) {
	char *input = NULL;
	int event = 0;
	switch ( *fmt ) {
	case 'c':
		event = fgetc( stdin );
		if ( event == EOF )
			return EOF;
		switch ( event ) {
		case '\0': asprintf( &input, "'\\0'" ); break;
		case '\t': asprintf( &input, "'\\t'" ); break;
		case '\n': asprintf( &input, "'\\n'" ); break;
		case '\'': asprintf( &input, "'\\\''" ); break;
		case '\\': asprintf( &input, "'\\\\'" ); break;
		default:
			if ( is_printable( event ) ) asprintf( &input, "'%c'", event );
			else asprintf( &input, "'\\x%.2X'", event ); }
		break;
	default:
		input = bm_read( stdin );
		if ( !input ) return EOF; }

	bm_instantiate_input( arg, input, ctx );
	free( input );
	return 0; }

//---------------------------------------------------------------------------
//	bm_read
//---------------------------------------------------------------------------
static char *
bm_read( FILE *stream ) {
	CNIO io;
	io_init( &io, stream, NULL, IOStreamInput, NULL );

	BMParseData data;
	memset( &data, 0, sizeof(BMParseData) );
        data.io = &io;
	bm_parse_init( &data, BM_INPUT );
	//-----------------------------------------------------------------
	int event = 0;
	do {	event = io_read( &io, event );
		if ( io.errnum ) data.errnum = io_report( &io );
		else data.state = bm_parse_load( event, BM_INPUT, &data, NULL );
		} while ( strcmp( data.state, "" ) && !data.errnum );
	//-----------------------------------------------------------------
	char *input;
	if ( !data.errnum ) {
		input = StringFinish( data.string, 0 );
		StringReset( data.string, CNStringMode ); }
	else input = NULL;

	bm_parse_exit( &data );
	io_exit( &io );
	return input; }

//===========================================================================
//	bm_outputf
//===========================================================================
static int bm_output( FILE *, char *fmt, char *expression, BMContext *);

int
bm_outputf( FILE *stream, char *fmt, listItem *args, BMContext *ctx )
/*
	Assumption: fmt starts and finishes with \" or \0
*/ {
	int delta; char_s q;
	if ( *fmt ) {
		fmt++; // skip opening double-quote
		while ( *fmt ) {
			switch ( *fmt ) {
			case '"':
				goto RETURN;
			case '%':
				if ( !fmt[1] ) break;
				else if ( fmt[1]=='%' )
					fprintf( stream, "%%" );
				else if ((args)) {
					char *arg = args->ptr;
					bm_output( stream, fmt+1, arg, ctx );
					args = args->next; }
				fmt += 2;
				break;
			default:
				if (( delta=charscan( fmt, &q ) )) {
					fprintf( stream, "%c", q.value );
					fmt += delta; }
				else fmt++; } } }
	else if (( args )) {
		for ( listItem *i=args; i!=NULL; i=i->next ) {
			bm_output( stream, DEFAULT_FMT, i->ptr, ctx );
			fprintf( stream, "\n" ); } }
	else fprintf( stream, "\n" );
RETURN:
	return 0; }

//---------------------------------------------------------------------------
//	bm_output
//---------------------------------------------------------------------------
static BMQueryCB output_CB;
static int
bm_output( FILE *stream, char *fmt, char *arg, BMContext *ctx )
/*
	outputs arg-expression's results
	note that we rely on bm_query to eliminate doublons
*/ {
	// special case: single quote & fmt "s" or "$"
	switch ( *arg ) {
	case '\'':
		if ( *fmt==*DEFAULT_FMT ) break;
		for ( char_s q; charscan( arg+1, &q ); ) {
			fprintf( stream, "%c", q.value );
			break; }
		return 0;
	case '"':
		for ( char *c=arg+1; *c!='"'; c++ )
			fprintf( stream, "%c", *c );
		return 0; }

	OutputData data;
	data.stream = stream;
	data.fmt = fmt;
	data.first = 1;
	data.last = NULL;

	// special case: EEnoRV as-is
	if ( !strncmp(arg,"%<",2) && strmatch("s$_",*fmt) && !p_filtered(arg) )
		return eenov_output( arg, ctx, &data );
	else if ( *fmt=='$' && !strncmp(arg,"%((?,...)",9) ) 
		data.fmt = ".";

	bm_query( BM_CONDITION, arg, ctx, output_CB, &data );
	return bm_out_last( &data, BMContextDB(ctx) ); }

static BMQTake
output_CB( CNInstance *e, BMContext *ctx, void *user_data ) {
	bm_out_put( user_data, e, BMContextDB(ctx) );
	return BMQ_CONTINUE; }

//===========================================================================
//	bm_out_put / bm_out_last
//===========================================================================
void
bm_out_put( OutputData *data, CNInstance *f, CNDB *db ) {
	CNInstance *e = data->last;
	if (( e )) {
		char *fmt = (*data->fmt=='.') ? "$" : data->fmt;
		db_outputf( db, data->stream,
			*fmt=='$' ? "%s" : data->first ?
				*fmt=='s' ? "\\{ %s" : "{ %_" :
				*fmt=='s' ? ", %s" : ", %_", e );
		data->first = 0; }
	data->last = (*data->fmt=='.') ? CNSUB(f,1) : f; }

int
bm_out_last( OutputData *data, CNDB *db ) {
	CNInstance *e = data->last;
	if ( !e ) return 0;
	else {
		char *fmt = (*data->fmt=='.') ? "$" : data->fmt;
		db_outputf( db, data->stream,
			*fmt=='$' ? "%s" : data->first ?
				*fmt=='s' ? "%s" : "%_" :
				*fmt=='s' ? ", %s }" : ", %_ }", e );
		return 1; } }

