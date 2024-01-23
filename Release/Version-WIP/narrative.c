#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "narrative.h"
#include "string_util.h"

//===========================================================================
//	newNarrative / freeNarrative
//===========================================================================
CNNarrative *
newNarrative( void ) {
	return (CNNarrative *) newPair( NULL, newOccurrence( ROOT ) ); }

void
freeNarrative( CNNarrative *narrative ) {
	if (( narrative )) {
		char *proto = narrative->proto;
		if (( proto )) free( proto );
		freeOccurrence( narrative->root );
		freePair((Pair *) narrative ); } }

//===========================================================================
//	newOccurrence / freeOccurrence
//===========================================================================
CNOccurrence *
newOccurrence( int type ) {
	union { int value; void *ptr; } icast;
	icast.value = type;
	Pair *data = newPair( icast.ptr, NULL );
	return (CNOccurrence *) newPair( data, NULL ); }

void
freeOccurrence( CNOccurrence *occurrence ) {
	if ( !occurrence ) return;
	listItem *i = newItem( occurrence ), *stack = NULL;
	for ( ; ; ) {
		occurrence = i->ptr;
		listItem *j = occurrence->sub;
		if (( j )) { addItem( &stack, i ); i=j; }
		else for ( ; ; ) {
			char *expression = occurrence->data->expression;
			if (( expression )) free( expression );
			freePair((Pair *) occurrence->data );
			freePair((Pair *) occurrence );
			if (( i->next )) {
				i = i->next;
				break; }
			else if (( stack )) {
				i = popListItem( &stack );
				occurrence = i->ptr;
				freeListItem( &occurrence->sub ); }
			else {
				freeItem( i );
				return; } } } }

//===========================================================================
//	narrative_reorder
//===========================================================================
void
narrative_reorder( CNNarrative *narrative ) {
	if ( !narrative ) return;
	CNOccurrence *occurrence = narrative->root;
	listItem *i = newItem( occurrence ), *stack = NULL;
	for ( ; ; ) {
		occurrence = i->ptr;
		listItem *j = occurrence->sub;
		if (( j )) { addItem( &stack, i ); i=j; }
		else for ( ; ; ) {
			if (( i->next )) {
				i = i->next;
				break; }
			else if (( stack )) {
				i = popListItem( &stack );
				occurrence = i->ptr;
				reorderListItem( &occurrence->sub ); }
			else {
				freeItem( i );
				return; } } } }

//===========================================================================
//	narrative_output
//===========================================================================
static inline void fprint_expr( FILE *, char *, int type, int level );
#define TAB( level ) \
	for ( int k=0; k<level; k++ ) fprintf( stream, "\t" );

int
narrative_output( FILE *stream, CNNarrative *narrative, int level ) {
	if ( narrative == NULL ) {
		fprintf( stderr, "Error: narrative_output: No narrative\n" );
		return 0; }
	char *proto = narrative->proto;
	if (( proto )) {
		if ( !is_separator( *proto ) )
			fprintf( stream, ": " );
		fprintf( stream, "%s\n", proto ); }
	CNOccurrence *occurrence = narrative->root;

	listItem *i = newItem( occurrence ), *stack = NULL;
	for ( ; ; ) {
		occurrence = i->ptr;
		int type = occurrence->data->type;
		char *expression = occurrence->data->expression;

		TAB( level );
		if ( type==ROOT ) ;
		else if ( type==ELSE )
			fprintf( stream, "else\n" );
		else {
			if ( type&ELSE ) fprintf( stream, "else " );
			fprintf( stream, type&IN ? "in " :
					 type&(ON|ON_X) ? "on " :
					 type&(DO|INPUT|OUTPUT) ? "do " :
					 type&PER ? "per " : "" );
			fprint_expr( stream, expression, type, level );
			fprintf( stream, "\n" ); }

		listItem *j = occurrence->sub;
		if (( j )) { addItem( &stack, i ); i=j; level++; }
		else for ( ; ; ) {
			if (( i->next )) {
				i = i->next;
				occurrence = i->ptr;
				break; }
			else if (( stack )) {
				i = popListItem( &stack );
				level--; }
			else {
				freeItem( i );
				return 0; } } } }

//---------------------------------------------------------------------------
//	fprint_expr
//---------------------------------------------------------------------------
#include "fprint_expr.h"

static inline void
fprint_expr( FILE *stream, char *expression, int type, int level ) {
	union { void *ptr; int value; } icast;
	if ( !(type&DO) ) {
		fprintf( stream, "%s", expression );
		return; }

	listItem *stack = NULL;
	int count=0, carry=0, ground=level;
	for ( char *p=expression; *p; p++ ) {
		switch ( *p ) {
		case '!':
			if ( p[1]!='!' )
				fprintf( stream, "!" );
			else {
				fprintf( stream, "!!" );
				p+=2;
				while ( !is_separator(*p) )
					fprintf( stream, "%c", *p++ );
				if ( *p=='(' && p[1]!=')' ) {
					fprintf( stream, "(" );
					level++; carry=1; count=0;
					RETAB( level ) } }
			break;
		case '|':
			if ( p[1]=='{' ) {
				fprintf( stream, " | {" );
				push( PIPE_LEVEL, level++, count );
				RETAB( level ); p++; }
#ifdef TEST_PIPE
			else if ( strmatch( "!?(", p[1] ) ) { 
#else
			else if ( strmatch( "!?", p[1] ) ) { 
#endif
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
			if ( type==PIPE_LEVEL &&
			     !strmatch( ",)", p[1] ) && (stack))
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
					if (carry||(stack)) RETAB( level )
					else fprintf( stream, " " ); }
			break;
		case ')':
			if ( strmatch( "({", p[1] ) ) {
				/* special case : loop bgn
				   if current, PIPE was pushed at | followed by ?
				   we are now at the closing ) of ?:(_) knowing
				   that either {_} or (_) follows
				   if (_) we shall pop PIPE at next closing )
				   if {_} we retype PIPE into PIPE_LEVEL |{
				*/
				fprintf( stream, ") " );
				count--;
				if ( p[1]=='{' && test_PIPE(count) ) {
					fprintf( stream, "{ " );
					retype( PIPE_LEVEL );
					p++; } }
			else if ( test_PIPE(count-1) ) {
				// special case: closing |(_)
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

//===========================================================================
//	cnIniOutput
//===========================================================================
#include "parser.h"
static int output_CB( BMParseOp, BMParseMode, void * );

int
cnIniOutput( FILE *output_stream, char *path, int level ) {
	FILE *stream = fopen( path, "r" );
	if ( !stream ) {
		fprintf( stderr, "B%%: Error: no such file or directory: '%s'\n", path );
		return -1; }

	union { void *ptr; int value; } icast;
	icast.value = level;
	Pair *entry = newPair( output_stream, icast.ptr );

	CNIO io;
	io_init( &io, stream, path, IOStreamFile );

	BMParseData data;
	memset( &data, 0, sizeof(BMParseData) );
	data.entry = entry;
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
	union { void *ptr; int value; } icast;
	BMParseData *data = user_data;
	FILE *stream = data->entry->name;
	icast.ptr = data->entry->value;
	int level = icast.value;
	if ( op==ExpressionTake ) {
		TAB( level );
		char *expression = StringFinish( data->string, 0 );
		fprint_expr( stream, expression, DO, level );
		StringReset( data->string, CNStringAll );
		fprintf( stream, "\n" ); }
	return 1; }

