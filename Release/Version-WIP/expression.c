#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "database.h"
#include "expression.h"
#include "instantiate.h"
#include "story.h"
#include "eenov.h"
#include "parser.h"
#include "fprint_expr.h"

// #define DEBUG

//===========================================================================
//	bm_feel
//===========================================================================
CNInstance *
bm_feel( int type, char *expression, BMContext *ctx ) {
	switch ( type ) {
	case BM_CONDITION: // special case: EEnoRV as-is
		if ( !strncmp(expression,"%<",2) && !p_filtered(expression) ) {
			return eenov_lookup( ctx, NULL, expression ); }
		// no break
	default:
		return bm_query( type, expression, ctx, NULL, NULL ); } }

//===========================================================================
//	bm_scan
//===========================================================================
static BMQueryCB scan_CB;
listItem *
bm_scan( char *expression, BMContext *ctx ) {
	listItem *results = NULL;
	if ( !strncmp( expression, "*^", 2 ) ) {
		CNInstance *e = bm_context_lookup( ctx, expression );
		if (( e )) results = newItem( e ); }
	else {
		bm_query( BM_CONDITION, expression, ctx, scan_CB, &results );
		reorderListItem( &results ); }
	return results; }

static BMQTake
scan_CB( CNInstance *e, BMContext *ctx, void *user_data ) {
	listItem **results = user_data;
	addItem( results, e );
	return BMQ_CONTINUE; }

//===========================================================================
//	bm_tag_register
//===========================================================================
int
bm_tag_register( char *expression, char *p, BMContext *ctx ) {
	for ( ; ; ) {
		registryRegister( ctx, expression, NULL );
		if ( *p==' ' ) {
			expression = p+3;
			p = p_prune( PRUNE_IDENTIFIER, expression ); }
		else return 1; } }

//===========================================================================
//	bm_tag_traverse
//===========================================================================
static BMQueryCB continue_CB;
static int err_tag( char * );
int
bm_tag_traverse( char *expression, char *p, BMContext *ctx )
/*
	Assumption: p: ~{_} or {_}
	Note that we do not free tag entry in context registry
*/ {
	Pair *entry = registryLookup( ctx, expression );
	if ( !entry ) return err_tag( expression );
	int released = ( *p=='~' ? (p++,1) : 0 );
	Pair *current = registryRegister( ctx, "^.", NULL );
	listItem *next_i, *last_i=NULL;
	listItem **entries = (listItem **) &entry->value;
	for ( listItem *i=*entries; i!=NULL; i=next_i ) {
		next_i = i->next;
		current->value = i->ptr;
		int clip = released;
		for ( char *q=p; *q++!='}'; q=p_prune(PRUNE_LEVEL,q) )
			bm_query( BM_CONDITION, q, ctx, continue_CB, &clip );
		if (( clip )) clipListItem( entries, i, last_i, next_i );
		else last_i = i; }
	registryDeregister( ctx, "^." );
	return 1; }

static BMQTake
continue_CB( CNInstance *e, BMContext *ctx, void *user_data ) {
	return BMQ_CONTINUE; }

static int
err_tag( char *expression ) {
	fprintf( stderr, ">>>>> B%%: error: "
		"list identifier unknown in expression\n"
			"\t\t.%%%s\n"
		"\t<<<<< instruction ignored\n", expression );
	return 0; }

//===========================================================================
//	bm_tag_inform
//===========================================================================
static BMQueryCB inform_CB;
int
bm_tag_inform( char *expression, char *p, BMContext *ctx )
/*
	Assumption: p: <_>
*/ {
	Pair *entry = registryLookup( ctx, expression );
	if ( !entry ) entry = registryRegister( ctx, expression, NULL );
	for ( char *q=p; *q++!='>'; q=p_prune(PRUNE_LEVEL,q) )
		bm_query( BM_CONDITION, q, ctx, inform_CB, &entry->value );
	return 1; }

static BMQTake
inform_CB( CNInstance *e, BMContext *ctx, void *user_data ) {
	addIfNotThere((listItem **) user_data, e );
	return BMQ_CONTINUE; }

//===========================================================================
//	bm_tag_clear
//===========================================================================
static freeRegistryCB clear_CB;
int
bm_tag_clear( char *expression, BMContext *ctx )
/*
	Assumption: expression: identifier~
	Note that we do free tag entry in context registry
*/ {
	Pair *entry = registryLookup( ctx, expression );
	if ( !entry ) return err_tag( expression );
	registryCBDeregister( ctx, clear_CB, expression );
	return 1; }

static void
clear_CB( Registry *registry, Pair *entry ) {
	freeListItem((listItem **) &entry->value ); }

//===========================================================================
//	bm_release
//===========================================================================
static BMQueryCB release_CB;
void
bm_release( char *expression, BMContext *ctx ) {
	CNDB *db = BMContextDB( ctx );
	bm_query( BM_CONDITION, expression, ctx, release_CB, db ); }

static BMQTake
release_CB( CNInstance *e, BMContext *ctx, void *user_data ) {
	db_deprecate( e, (CNDB *) user_data );
	return BMQ_CONTINUE; }

//===========================================================================
//	bm_inputf
//===========================================================================
static int bm_input( int type, char *arg, BMContext * );
#define DEFAULT_TYPE '_'
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
					event = bm_input( fmt[1], arg, ctx );
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
			event = bm_input( DEFAULT_TYPE, arg, ctx );
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
bm_input( int type, char *arg, BMContext *ctx ) {
	char *input = NULL;
	int event = 0;
	switch ( type ) {
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
	io_init( &io, stream, NULL, IOStreamInput );

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
static int bm_output( FILE *, int type, char *expression, BMContext *);
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
					bm_output( stream, fmt[1], arg, ctx );
					args = args->next; }
				fmt+=2; break;
			default:
				if (( delta=charscan( fmt, &q ) )) {
					fprintf( stream, "%c", q.value );
					fmt += delta; }
				else fmt++; } } }
	else if (( args )) {
		for ( listItem *i=args; i!=NULL; i=i->next ) {
			bm_output( stream, DEFAULT_TYPE, i->ptr, ctx );
			fprintf( stream, "\n" ); } }
	else fprintf( stream, "\n" );
RETURN:
	return 0; }

//---------------------------------------------------------------------------
//	bm_output
//---------------------------------------------------------------------------
static BMQueryCB output_CB;
static int
bm_output( FILE *stream, int type, char *arg, BMContext *ctx )
/*
	outputs arg-expression's results
	note that we rely on bm_query to eliminate doublons
*/ {
	// special case: single quote & type 's' or '$'
	if ( *arg=='\'' && type!=DEFAULT_TYPE ) {
		for ( char_s q; charscan( arg+1, &q ); ) {
			fprintf( stream, "%c", q.value );
			break; }
		return 0; }

	OutputData data = { stream, type, 1, NULL };

	// special case: EEnoRV as-is
	if ( !strncmp(arg,"%<",2) && !p_filtered(arg) )
		return eenov_output( arg, ctx, &data );

	bm_query( BM_CONDITION, arg, ctx, output_CB, &data );
	return bm_out_flush( &data, BMContextDB(ctx) ); }

static BMQTake
output_CB( CNInstance *e, BMContext *ctx, void *user_data ) {
	bm_out_put( user_data, e, BMContextDB(ctx) );
	return BMQ_CONTINUE; }

//===========================================================================
//	bm_out_put / bm_out_flush
//===========================================================================
void
bm_out_put( OutputData *data, CNInstance *e, CNDB *db ) {
	if (( data->last )) {
		FILE *stream = data->stream;
		if ( data->type=='$' )
			db_outputf( stream, db, "%_", data->last );
		else {
			if ( data->first ) {
				switch ( data->type ) {
				case 's': fprintf( stream, "\\{" ); break;
				default: fprintf( stream, "{" ); }
				data->first = 0; }
			else fprintf( stream, "," );
			db_outputf( stream, db, " %_", data->last ); } }
	data->last = e; }

int
bm_out_flush( OutputData *data, CNDB *db ) {
	FILE *stream = data->stream;
	if ( data->type=='$' )
		db_outputf( stream, db, "%_", data->last );
	else if ( !data->first )
		db_outputf( stream, db, ", %_ }", data->last );
	else if ( data->type=='s' )
		db_outputf( stream, db, "%s", data->last );
	else	db_outputf( stream, db, "%_", data->last );
	return 0; }

//===========================================================================
//	fprint_expr
//===========================================================================
void
fprint_expr( FILE *stream, char *expression, int level ) {
#if 0
fprintf( stream, "%s", expression );
return;
#endif
	listItem *stack = NULL, *nb = NULL;
	int count=0, carry=0, ground=level;
	for ( char *p=expression; *p; p++ ) {
		switch ( *p ) {
		case '!':
			if ( p[1]=='^' ) {
				fprintf( stream, "!^" );
				add_item( &nb, count );
				p++; }
			else if ( p[1]=='!' ) {
				fprintf( stream, "!!" );
				if ( p[2]=='|' ) p++;
				else {
					p+=2;
					while ( !is_separator(*p) )
						fprintf( stream, "%c", *p++ );
					if ( *p=='(' && p[1]!=')' ) {
						fprintf( stream, "(" );
						level++; carry=1; count=0;
						RETAB( level ) } } }
			else fprintf( stream, "!" );
			break;
		case '|':
			if ( p[1]=='{' ) {
				fprintf( stream, " | {" );
				push( PIPE_LEVEL, level++, count );
				RETAB( level ); p++; }
			else if ( strmatch( PIPE_CND, p[1] ) ) { 
				fprintf( stream, " |" );
				push( PIPE, level++, count );
				RETAB( level ) }
			else
				fprintf( stream, "|" );
			break;
		case '{': // not following '|'
			fprintf( stream, "{ " );
			push( LEVEL, level, count );
			break;
		case '}':
			fprintf( stream, " }" );
			int type = pop( &level, &count );
			if ( type==PIPE_LEVEL && (stack) && !strmatch(",)",p[1]) )
				RETAB( level )
			for ( ; p[1]=='}'; p++ ) {
				pop( &level, &count );
				fprintf( stream, "}" ); }
			break;
		case ',':
			fprintf( stream, "," );
			if ( test(COUNT)==count )
				switch ( test(TYPE) ) {
				case PIPE:
				case LEVEL:
					fprintf( stream, " " );
					break;
				default:
					if ( level==ground ) {}
					else if (carry||(stack)) RETAB( level )
					else fprintf( stream, " " ); }
			break;
		case ')':
			if (( p[1]=='(') && ( nb ) && cast_i(nb->ptr)==(count-1) ) {
				// closing first term of binary !^:(_)(_)
				fprintf( stream, ")" );
				popListItem( &nb );
				count--; }
			else if ( strmatch( "({", p[1] ) ) {
				// closing loop bgn ?:(_)
				fprintf( stream, ")" );
				if ((stack)||carry) fprintf( stream, " " );
				if ( p[1]=='{' && test_PIPE(count-1) )
					retype( PIPE_LEVEL );
				count--; }
			else if ( test_PIPE(count-1) ) {
				// closing |(_)
				int retab = 1;
				fprintf( stream, ")" );
				pop( &level, &count );
				while ( strmatch("),",p[1]) ) {
					p++;
					if ( *p==',' ) {
						fprintf( stream, "," );
						if ( !count ) RETAB( level )
						else fprintf( stream, " " );
						break; }
					else if (( stack )) {
						fprintf( stream, ")" );
						if ( retab ) {
							RETAB( level )
							retab = 0; }
						if ( test_PIPE(count-1) )
							pop( &level, &count );
						else count--; }
					else fprint_close( stream ) }
				if ( p[1]=='}' ) retype( PIPE_LEVEL ); }
			else fprint_close( stream );
			break;
		//--------------------------------------------------
		//	special cases: list, literal, format
		//--------------------------------------------------
		case '.':
			if ( strncmp(p,"...):",5) ) {
				fprintf( stream, "%c", *p );
				break; }
			fprintf( stream, "..." );
			p+=3; count--;
			// no break
		case '(':
			if ( p[1]!=':' ) {
				count++;
				fprintf( stream, "(" );
				break; }
			// no break
		case '"':
			for ( char *q=p_prune(PRUNE_TERM,p); p!=q; p++ )
				fprintf( stream, "%c", *p );
			p--; break;
		default:
			fprintf( stream, "%c", *p ); } } }

