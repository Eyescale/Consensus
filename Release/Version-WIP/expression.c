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
#include "fprint_expr.h"
#include "errout.h"

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
//	bm_scan, bm_forescan
//===========================================================================
static BMQueryCB scan_CB;
listItem *
bm_scan( char *expression, BMContext *ctx ) {
	listItem *results = NULL;
	if ( !strncmp( expression, "*^", 2 ) ) {
		CNInstance *e = BMVal( ctx, expression );
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
//	bm_switch / bm_case
//===========================================================================
// see include/expression.h

//===========================================================================
//	bm_tag_register
//===========================================================================
int
bm_tag_register( char *tag, char *p, BMContext *ctx ) {
	for ( ; ; ) {
		bm_tag( ctx, tag, NULL );
		if ( *p==' ' ) {
			tag = p+3; // skip ' .%'
			p = p_prune( PRUNE_IDENTIFIER, tag ); }
		else return 1; } }

//===========================================================================
//	bm_tag_traverse
//===========================================================================
static BMQueryCB continue_CB;
int
bm_tag_traverse( char *expression, char *p, BMContext *ctx )
/*
	Assumption: p: ~{_} or {_}
	Note that we do not free tag entry in context registry
*/ {
	Pair *entry = BMTag( ctx, expression );
	if ( !entry ) {
//		errout( ExpressionTagUnknown, expression );
		return 0; }
	int released = ( *p=='~' ? (p++,1) : 0 );

	Pair *current = Mset( ctx, NULL );
	listItem *next_i, *last_i=NULL;
	listItem **entries = (listItem **) &entry->value;
	for ( listItem *i=*entries; i!=NULL; i=next_i ) {
		next_i = i->next;
		current->value = i->ptr;
		int clip = released;
		for ( char *q=p; *q++!='}'; q=p_prune(PRUNE_LEVEL,q) )
			bm_query( BM_CONDITION, q, ctx, continue_CB, &clip );
		if (( clip ))
			clipListItem( entries, i, last_i, next_i );
		else last_i = i; }
	Mclr( ctx );

	return 1; }

static BMQTake
continue_CB( CNInstance *e, BMContext *ctx, void *user_data ) {
	return BMQ_CONTINUE; }

//===========================================================================
//	bm_tag_inform
//===========================================================================
static BMQueryCB inform_CB;
int
bm_tag_inform( char *expression, char *p, BMContext *ctx )
/*
	Assumption: p: <_>
*/ {
	Pair *entry = BMTag( ctx, expression );
	if ( !entry ) entry = bm_tag( ctx, expression, NULL );

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
	Pair *entry = BMTag( ctx, expression );
	if ( !entry ) {
//		errout( ExpressionTagUnknown, expression );
		return 0; }
	bm_untag( ctx, expression );
	return 1; }

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
					args = args->next;
					if ( fmt[1]=='(' ) {
						fmt = prune_xsub(fmt+1) + 1;
						break; } }
				fmt+=2; break;
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

	OutputData data = { stream, fmt, 1, NULL };

	// special case: EEnoRV as-is
	if ( !strncmp(arg,"%<",2) && strmatch("s$_",*fmt) && !p_filtered(arg) )
		return eenov_output( arg, ctx, &data );

	bm_query( BM_CONDITION, arg, ctx, output_CB, &data );
	return bm_out_last( &data, BMContextDB(ctx) ); }

static BMQTake
output_CB( CNInstance *e, BMContext *ctx, void *user_data ) {
	bm_out_put( user_data, e, BMContextDB(ctx) );
	return BMQ_CONTINUE; }

//===========================================================================
//	bm_out_put / bm_out_last
//===========================================================================
static inline int out_pass( int pass, char *, OutputData *, CNDB * );

void
bm_out_put( OutputData *data, CNInstance *e, CNDB *db ) {
	char *fmt = data->fmt;
	if ( *fmt=='(' ) {
		listItem *xpn = xpn_make( fmt );
		if ( !xpn ) return;
		e = xsub( e, xpn );
		freeListItem( &xpn );
		if ( !e ) return;
		fmt = prune_xsub( fmt ); }
	out_pass( 0, fmt, data, db );
	data->last = e; }

int
bm_out_last( OutputData *data, CNDB *db ) {
	char *fmt = data->fmt;
	if ( *fmt=='(' ) fmt = prune_xsub( fmt );
	return out_pass( 1, fmt, data, db ); }

static inline int
out_pass( int is_last, char *fmt, OutputData *data, CNDB *db ) {
	CNInstance *e = data->last;
	if ( !e ) return 0;
	FILE *stream = data->stream;
	if ( *fmt=='$' )
		db_outputf( db, stream, "%s", e );
	else {
		switch ( is_last ) {
		case 0: // not last pass
			db_outputf( db, stream, (( data->first ) ?
				*fmt=='s' ? "\\{ %s" : "{ %_" :
				*fmt=='s' ? ", %s" : ", %_"
				), e );
			data->first = 0;
			break;
		default:
			db_outputf( db, stream, (( data->first ) ?
				*fmt=='s' ? "%s" : "%_" :
				*fmt=='s' ? ", %s }" : ", %_ }"
				), e ); } }
	return 1; }

//===========================================================================
//	fprint_expr
//===========================================================================
void
fprint_expr( FILE *stream, char *expression, int level ) {
	listItem *stack = NULL, *nb = NULL; // newborn
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
				if ( p[2]=='|' ) p++; else {
					p+=2;
					while ( !is_separator(*p) )
						putc( *p++, stream );
					if ( *p=='(' && p[1]!=')' ) {
						putc( '(', stream );
						level++; carry=1; count=0;
						RETAB( level ) } } }
			else putc( '!', stream );
			break;
		case '|':
			switch ( p[1] ) {
			case '{':
				fprintf( stream, " |{" );
				push( PIPE_LEVEL, level++, count );
				RETAB( level );
				p++; break;
			case '!':
			case '?':
				fprintf( stream, " |" );
				push( PIPE, level++, count );
				RETAB( level )
				break;
			case ')':
				putc( '|', stream );
				putc( ' ', stream );
				break;
			default:
				putc( '|', stream ); }
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
				putc( '}', stream ); }
			break;
		case ',':
			putc( ',', stream );
			if ( test_(COUNT)==count )
				switch ( test_(TYPE) ) {
				case PIPE:
				case LEVEL:
					putc( ' ', stream );
					break;
				default:
					if ( level==ground ) {}
					else if (carry||(stack)) RETAB( level )
					else putc( ' ', stream ); }
			break;
		case ')':
			if (( p[1]=='(') && ( nb ) && cast_i(nb->ptr)==(count-1) ) {
				// closing first term of binary !^:(_)(_)
				putc( ')', stream );
				popListItem( &nb );
				count--; }
			else if ( strmatch( "({", p[1] ) ) {
				// closing loop bgn ?:(_)
				putc( ')', stream );
				if ((stack)||carry) putc( ' ', stream );
				if ( p[1]=='{' && test_PIPE(count-1) )
					retype( PIPE_LEVEL );
				count--; }
			else if ( test_PIPE(count-1) ) {
				// closing |(_)
				int retab = 1;
				putc( ')', stream );
				pop( &level, &count );
				while ( strmatch("),",p[1]) ) {
					p++;
					if ( *p==',' ) {
						putc( ',', stream );
						if ( !count ) RETAB( level )
						else putc( ' ', stream );
						break; }
					else if (( stack )) {
						putc( ')', stream );
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
				putc( *p, stream );
				break; }
			fprintf( stream, "..." );
			p+=3; count--;
			// no break
		case '(':
			if ( p[1]!=':' ) {
				count++;
				putc( '(', stream );
				break; }
			// no break
		case '"':
			for ( char *q=p_prune(PRUNE_TERM,p); p!=q; p++ )
				putc( *p, stream );
			p--; break;
		default:
			putc( *p, stream ); } }
	if (( stack )||( nb )) {
//		fprintf( stderr, "fprint_expr: Memory Leak: stack\n" );
		do pop( &level, &count ); while (( stack ));
		freeListItem( &nb ); }
	return; }

//===========================================================================
//	fprint_output
//===========================================================================
void
fprint_output( FILE *stream, char *p, int level ) {
	putc( *p++, stream ); // output '>'
	if ( *p=='"' ) {
		char *q = p_prune( PRUNE_IDENTIFIER, p );
		do putc( *p++, stream ); while ( p!=q ); }
	if ( !*p ) return;
	putc( *p++, stream ); // output ':'
	if ( *p=='<' ) {
		fprintf( stream, "%c ", *p++ );
		for ( ; ; ) {
			char *q = p_prune( PRUNE_LEVEL, p );
			do putc( *p++, stream ); while ( p!=q );
			switch ( *p++ ) {
			case ',':
				fprintf( stream, ", " );
				break;
			default:
				fprintf( stream, " >" );
				return; } } }
	else fprintf( stream, "%s", p ); }

