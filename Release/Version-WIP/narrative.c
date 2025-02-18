#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "narrative.h"
#include "string_util.h"
#include "errout.h"

#include "fprint_expr_traversal.h"

//===========================================================================
//	newNarrative / freeNarrative
//===========================================================================
CNNarrative *
newNarrative( void ) {
	return (CNNarrative *) newPair( NULL, newOccurrence( ROOT ) ); }

void
freeNarrative( CNNarrative *narrative ) {
	char *proto = narrative->proto;
	if (( proto ) && strcmp(proto,""))
		free( proto );
	freeOccurrence( narrative->root );
	freePair((Pair *) narrative ); }

//===========================================================================
//	newOccurrence / freeOccurrence
//===========================================================================
CNOccurrence *
newOccurrence( int type ) {
	Pair *data = newPair( cast_ptr( type ), NULL );
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
			if ( occurrence->data->type!=ROOT ) {
				char *expression = occurrence->data->expression;
				if (( expression )) free( expression ); }
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
void db_arena_output( FILE *, char *, char * ); // cf. db_arena.c

static inline void output_proto( FILE *, char * );
static inline int output_cmd( FILE *, int, int );
static inline void output_expr( FILE *, char *, int, int );

int
narrative_output( FILE *stream, CNNarrative *narrative, int level ) {
	if ( narrative == NULL ) return !!errout( NarrativeOutputNone );
	output_proto( stream, narrative->proto );
	CNOccurrence *occurrence = narrative->root;
	listItem *i = newItem( occurrence ), *stack = NULL;
	for ( ; ; ) {
		occurrence = i->ptr;
		int type = cast_i( occurrence->data->type );
		char *expression = occurrence->data->expression;
		if ( output_cmd( stream, type, level ) )
			output_expr( stream, expression, level, type );
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

static inline void
output_proto( FILE *stream, char *p ) {
	if ( !p ) return;
	else if ( *p==':' ) fprintf( stream, ".%s\n", p );
	else {
		do fputc( *p++, stream ); while ( !is_separator(*p) );
		if ( p[1]=='"' ) db_arena_output( stream, ":%_\n", p+1 );
		else fprintf( stream, "%s\n", p ); } }

#define if_( a, b ) if (type&(a)) fprintf(stream,b);
static inline int
output_cmd( FILE *stream, int type, int level ) {
	if ( type==ROOT )
		return 0;
	else {
		TAB( level );
		if ( type==ELSE ) {
			fprintf( stream, "else\n" );
			return 0; }
		else if ( !(type&CASE) ) {
			if_( ELSE, "else " )
			if_( EN, "en " )
			else if_( PER, "per " )
			else if_( IN, "in " )
			else if_( ON|ON_X, "on " )
			else if_( DO|INPUT|OUTPUT, "do " ) }
		return 1; } }

static inline void
output_expr( FILE *stream, char *p, int level, int type ) {
	if ( type&(DO|OUTPUT) )
		fprint_expr( stream, p, level, type );
	else if ( type&IN && !strncmp(p,"?:\"",3) )
		db_arena_output( stream, "?:%_", p+2 );
	else fprintf( stream, "%s", p );
	fprintf( stream, "\n" ); }

//===========================================================================
//	fprint_expr
//===========================================================================
typedef struct {
	FILE *stream;
	char *p;
	int level, piped;
	struct { char *p; int level; } ternary;
	struct { listItem *level, *flags; } stack;
	} fprintExprData;
static char *
	jump_start( fprintExprData *, char * );

void
fprint_expr( FILE *stream, char *p, int level, int type ) {
	fprintExprData data;
        memset( &data, 0, sizeof(data) );
	data.stream = stream;
	data.p = p;
	data.level = level;
	p = jump_start( &data, p );
	if ( *p=='"' ) {
		for ( char*q=data.p; q!=p; q++ )
			fputc( *q, stream );
		db_arena_output( stream, "%_", p );
		return; }
	else if ( *p && strlen(data.p)>=40 ) {
		BMTraverseData traversal;
		traversal.user_data = &data;
		traversal.stack = &data.stack.flags;
		traversal.done = TERNARY|INFORMED;
		if ( type&DO ) traversal.done |= LITERAL;
		fprint_expr_traversal( p, &traversal, FIRST ); }
	fprintf( stream, "%s", data.p ); }

static inline char * retab( fprintExprData *, char * );
static char *
jump_start( fprintExprData *data, char *p ) {
	switch ( *p ) {
	case '>':
		p++;
		if ( *p=='&' ) p++;
		if ( *p=='"' ) p = p_prune( PRUNE_IDENTIFIER, p );
		return *p ? p+1 : p;
	case ':':
		p = p_prune( PRUNE_LEVEL, p+1 );
		if ( !*p ) return p;
		if ( p[1]!='!' ) return p+1;
		p++; // skip ',' // no break
	case '!':
		p+=2;
		if ( is_separator(*p) ) return p;
		do p++; while ( !is_separator(*p) );
		if ( *p=='(' && strlen(data->p)>=40 )
			return retab( data, p );
	default:
		return p; } }

static inline char *
retab( fprintExprData *data, char *p ) {
	FILE *stream = data->stream;
	p++;
	for ( char *q=data->p; q!=p; q++ )
		fputc( *q, stream );
	fputc( '\n', stream );
	for ( int l=data->level++; (l--)>=0; )
		fputc( '\t', stream );
	return (( data->p=p )); }

//---------------------------------------------------------------------------
//	fprint_expr_traversal
//---------------------------------------------------------------------------
/*
	translate
		_<_,_>_ into _</_,/_>_
		_{_,_}_ into _{/_,/_}_
		_|{_,_}_ into _|{/_,/_}_
		_|_ into _|/_
		_(_?_:_)_ into _(_?/_:/_)_
	where
		/ means retab

	Notes
	1. each pipe retab increases the indentation level, whereas
	   vector or set retab increases it only once and consistently
	   restores it after each comma
	2. ternary separation is only effected at the first ternary level
*/
BMTraverseCBSwitch( fprint_expr_traversal )
case_( bgn_pipe_CB )
	if ( p[1]=='{' ) data->piped = 1;
	else retab( data, p );
	_break
case_( bgn_set_CB )
	if ( !is_f(LEVEL) && *p_prune(PRUNE_LEVEL,p+1)==',' ) {
		add_item( &data->stack.level, data->level );
		retab( data, p ); data->piped = 0; }
	else add_item( &data->stack.level, 0 );
	_break
case_( end_set_CB )
	popListItem( &data->stack.level );
	_break
case_( comma_CB )
	if ( is_f(SET|VECTOR) && !is_f(LEVEL|SUB_EXPR) ) {
		int level = cast_i( data->stack.level->ptr );
		if ( level ) {
			data->level = level;
			retab( data, p ); } }
	_break
case_( open_CB )
	if ( !data->ternary.p ) {
		p = p_prune( PRUNE_TERNARY, p );
		if ( *p=='?' && p[1]!=':' ) { // ignore (_?:_)
			char *ternary_p = p_prune( PRUNE_TERNARY, p );
			if ( ternary_p[1]!=')') { // ignore (_?_:)
				data->ternary.p = ternary_p;
				data->ternary.level = data->level;
				retab( data, p ); } } }
	_break
case_( filter_CB )
	if ( p==data->ternary.p && *p==':' ) {
		data->ternary.p = p_prune( PRUNE_TERNARY, p );
		data->level = data->ternary.level;
		retab( data, p ); }
	else if ( !strncmp(p,":|",2) && !strmatch("){",p[2]) )
		retab( data, p+1 );
	_break
case_( close_CB )
	if ( p==data->ternary.p )
		data->ternary.p = NULL;
	_break
BMTraverseCBEnd

