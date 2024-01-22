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
#define RETAB( level )	{ fprintf( stream, "\n" ); TAB( level ) }
#define stacktest(t)	((stack)&&((icast.ptr=stack->ptr),(t)))
#define add_cast(s,i)	add_item(s,-(i+1))
#define pop_cast(s)	-(pop_item(s)+1)

static inline void
fprint_expr( FILE *stream, char *expression, int type, int level )
/*
   Rules
	. carry( => level++ and RETAB
	. |{ => push level++ and count=0 and RETAB
	. |( => push level++ and -(count+1) and RETAB
	. { not following | => push count and level and output '{ '
	. } => pop count and level and
		if inside |{_}
			level-- and RETAB
		else if inside {_}
 			output ' }'
		group process subsequent '}'
	. , and count==base
		if inside |{_}
			output ',\n' and RETAB
		else if inside {_}
			output ', '
	. ) => count--
		if carry's closing ')'
			output ' )'
	. ( => count++
*/ {
	if ( !(type&DO) ) { fprintf( stream, "%s", expression ); return; }
	union { void *ptr; int value; } icast;
	listItem *stack = NULL;
	int count=0, carry=0;
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
					level++; carry=1;
					fprintf( stream, "(" );
					RETAB( level ) } }
			break;
		case '|':
			if ( p[1]=='{' ) {
				fprintf( stream, " | {" );
				add_item( &stack, level++ );
				add_item( &stack, (count=0) );
				RETAB( level ); p++; }
			else if ( p[1]=='(' ) { 
				fprintf( stream, " |" );
				add_item( &stack, level++ );
				add_cast( &stack, count );
				RETAB( level ) }
			else
				fprintf( stream, "|" );
			break;
		case '{': // not following '|'
			fprintf( stream, "{ " );
			add_item( &stack, level );
			add_item( &stack, count );
			break;
		case '}':
			count = pop_item( &stack );
			level = pop_item( &stack );
			fprintf( stream, " }" );
			if ((stack) && !strmatch(",)",p[1]) )
				RETAB( level )
			for ( ; p[1]=='}'; p++ ) {
				count = pop_item( &stack );
				level = pop_item( &stack );
				fprintf( stream, "}" ); }
			break;
		case ',':
			fprintf( stream, "," );
			if ( !count && (carry||(stack)) )
				RETAB( level )
			else if stacktest( icast.value==count )
				fprintf( stream, " " );
			break;
		case ')': ;
			// special case: closing |(_)
			if ( count==1 && stacktest( icast.value < 0 )) {
				int retab = 1;
				fprintf( stream, ")" );
				count = pop_cast( &stack );
				level = pop_item( &stack );
				while ( strmatch("),",p[1]) ) {
					p++;
					if ( *p==',' ) {
						fprintf( stream, "," );
						if stacktest( icast.value==count )
							fprintf( stream, " " );
						break; }
					else if (( stack )) {
						fprintf( stream, ")" );
						if ( retab ) {
							RETAB( level )
							retab = 0; }
						if stacktest( icast.value < 0 ) {
							count = pop_cast( &stack );
							level = pop_item( &stack ); }
						else if ( count ) count--; }
					else if ( carry ) {
						fprintf( stream, " )" );
						carry = 0; }
					else fprintf( stream, ")" ); } }
			else if ( count ) {
				fprintf( stream, ")" );
				count--; }
			else if (( stack ))
				fprintf( stream, ")" );
			else if ( carry ) {
				fprintf( stream, " )" );
				carry = 0; }
			else fprintf( stderr, ")" );
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

