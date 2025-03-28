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
static inline int output_cmd( FILE *, int, int, char * );
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
		if ( output_cmd( stream, type, level, expression ) )
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
	if ( !p || *p=='<' ) return;
	else if ( *p==':' ) fprintf( stream, ".%s\n", p );
	else {
		do fputc( *p++, stream ); while ( !is_separator(*p) );
		if ( p[1]=='"' ) db_arena_output( stream, ":%_\n", p+1 );
		else fprintf( stream, "%s\n", p ); } }

static inline int in_x( int type, char *p );
static inline int
output_cmd( FILE *stream, int type, int level, char *expression ) {
	if ( type==ROOT ) return 0;
	TAB( level );
	if ( type==ELSE ) {
		fprintf( stream, "else\n" );
		return 0; }
	else if ( type&CASE )
		return 1;
	if ( type&ELSE ) fprintf( stream, "else " );
	if ( type&EN ) fprintf( stream, "en " );
	else if	( in_x(type,expression) ) fprintf( stream, "in " );
	else if ( type&PER ) fprintf( stream, "per " );
	else if ( type&IN ) fprintf( stream, "in " );
	else if ( type&(ON|ON_X) ) fprintf( stream, "on " );
	else if ( type&(DO|INPUT|OUTPUT) ) fprintf( stream, "do " );
	return 1; }

static inline int in_x( int type, char *p ) {
	return	(type&(PER|ON))==(PER|ON) &&
		!strncmp(p,".%",2) &&
		!is_separator(p[2]) &&
		*p_prune(PRUNE_IDENTIFIER,p+3)==':'; }

static inline void
output_expr( FILE *stream, char *p, int level, int type ) {
	if ( type&(DO|OUTPUT) )
		fprint_expr( stream, p, level, type );
	else if ( type&(IN|ON) && !strncmp(p,"?:\"",3) )
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
	jump_start( fprintExprData *, char *, int * );

void
fprint_expr( FILE *stream, char *p, int level, int type ) {
	fprintExprData data;
        memset( &data, 0, sizeof(data) );
	data.stream = stream;
	data.p = p;
	data.level = level;
	int carry = 0;
	p = jump_start( &data, p, &carry );
	if ( *p=='"' ) {
		for ( char*q=data.p; q!=p; q++ )
			fputc( *q, stream );
		db_arena_output( stream, "%_", p );
		return; }
	else if ( *p && ( carry || strlen(data.p)>=40 )) {
		BMTraverseData traversal;
		traversal.user_data = &data;
		traversal.stack = &data.stack.flags;
		traversal.done = TERNARY|INFORMED;
		if ( type&DO ) traversal.done |= LITERAL;
		fprint_expr_traversal( p, &traversal, FIRST|carry );
		if ( carry ) popListItem( &data.stack.level ); }
	fprintf( stream, "%s", data.p ); }

static inline char * retab( fprintExprData *, char * );
static char *
jump_start( fprintExprData *data, char *p, int *carry ) {
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
		while ( !is_separator(*p) ) p++;
		if ( *p=='(' && strlen(data->p)>=40 ) {
			*carry = CARRY;
			add_item( &data->stack.level, data->level );
			return retab( data, p ); }
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
	else if is_f(LEVEL|SUB_EXPR|VECTOR|SET) retab( data, p );
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
	if ( is_f(SET|VECTOR|CARRY) && !is_f(LEVEL|SUB_EXPR) ) {
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
case_( wildcard_CB )
	if ( !strncmp(p,"?:",2) ) {
		p = p_prune( PRUNE_TERM, p );
		if ( *p=='(' ) retab( data, p-1 );
		_continue( p ); }
	_break;
BMTraverseCBEnd

