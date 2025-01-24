#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "narrative.h"
#include "string_util.h"
#include "errout.h"

#include "fprint_expr.h"
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
static inline void fprint_switch( FILE *, char *, int, int );

#define if_( a, b ) if (type&(a)) fprintf(stream,b);

int
narrative_output( FILE *stream, CNNarrative *narrative, int level ) {
	if ( narrative == NULL ) return !!errout( NarrativeOutputNone );
	char *proto = narrative->proto;
	if (( proto )) {
		if ( *proto==':' ) fprintf( stream, ".%s\n", proto );
		else fprintf( stream, "%s\n", proto ); }
	CNOccurrence *occurrence = narrative->root;
	listItem *i = newItem( occurrence ), *stack = NULL;
	for ( ; ; ) {
		occurrence = i->ptr;
		int type = cast_i( occurrence->data->type );
		char *expression = occurrence->data->expression;
		switch ( type ) {
		case ROOT: break;
		case ELSE:
			TAB( level );
			fprintf( stream, "else\n" );
			break;
		default:
			TAB( level );
			// output cmd
			if ( !(type&CASE) ) {
				if_( ELSE, "else " )
				if_( EN, "en " )
				else if_( PER, "per " )
				else if_( IN, "in " )
				else if_( ON|ON_X, "on " )
				else if_( DO|INPUT|OUTPUT, "do " ) }
			// output expr
			fprint_switch( stream, expression, level, type ); }
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
//	fprint_switch
//---------------------------------------------------------------------------
static inline void
fprint_switch( FILE *stream, char *expression, int level, int type ) {
	if ( strlen( expression ) < 40 )
		fprintf( stream, "%s", expression );
	else if ( type & DO )
		fprint_expr( stream, expression, level, type );
	else if ( type & OUTPUT )
		fprint_output( stream, expression, level );
	else fprintf( stream, "%s", expression );
	fprintf( stream, "\n" ); }

//===========================================================================
//	fprint_expr
//===========================================================================
typedef struct {
	FILE *stream;
	char *p;
	int level, type;
	struct { char *p; int level; } ternary;
	struct { listItem *level, *flags; } stack;
	} fprintExprData;
static char *
	carry_out( fprintExprData *, char * );

void
fprint_expr( FILE *stream, char *p, int level, int type ) {
	fprintExprData data;
        memset( &data, 0, sizeof(data) );
	data.stream = stream;
	data.p = p;
	data.level = level;
	data.type = type;

	// get carry out of the way
	p = carry_out( &data, p );
	if ( !p ) return;

	BMTraverseData traversal;
	traversal.user_data = &data;
	traversal.stack = &data.stack.flags;
	traversal.done = TERNARY|INFORMED;
	if ( type&DO ) traversal.done |= LITERAL;

	p = fprint_expr_traversal( p, &traversal, FIRST );
	for ( char *q=data.p; q!=p; q++ ) fputc( *q, stream ); }

static inline char * retab( fprintExprData *, char * );
static char *
carry_out( fprintExprData *data, char *p ) {
	switch ( *p ) {
	case ':':
		p = p_prune( PRUNE_LEVEL, p+1 );
		switch ( *p ) {
		case '\0':
			fprintf( data->stream, "%s", data->p );
			return NULL;
		case ',':
			if ( p[1]!='!' )
				return p+1;
			p++; }
		// no break
	case '!':
		p+=2;
		switch ( *p ) {
		case '\0':
			fprintf( data->stream, "%s", data->p );
			return NULL;
		default:
			if ( is_separator(*p) )
				return p; }
		do p++; while ( !is_separator(*p) );
		return retab( data, p );
	default:
		return p; } }

static inline char *
retab( fprintExprData *data, char *p ) {
	p++;
	FILE *stream = data->stream;
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
	2. ternary separation is only effected at the ternary base level
*/
BMTraverseCBSwitch( fprint_expr_traversal )
case_( bgn_pipe_CB )
	if ( p[1]!='{' ) retab( data, p );
	_break
case_( bgn_set_CB )
	int check = ( *p=='{' || ( *p=='<' && data->type&OUTPUT ));
	if ( check ) {
		char *partition = p_prune( PRUNE_LEVEL, p+1 );
		if ( *partition!=',' )
			add_item( &data->stack.level, 0 );
		else {
			add_item( &data->stack.level, data->level );
			retab( data, p ); } }
	_break
case_( end_set_CB )
	popListItem( &data->stack.level );
	_break
case_( comma_CB )
	int check = is_f(SET) || ( is_f(VECTOR) && data->type&OUTPUT );
	if ( check && !is_f(LEVEL|SUB_EXPR) ) {
		int level = cast_i( data->stack.level->ptr );
		if ( level ) {
			data->level = level;
			retab( data, p ); } }
	_break
case_( filter_CB )
	if ( p==data->ternary.p ) {
		data->ternary.p = NULL;
		data->level = data->ternary.level;
		retab( data, p ); }
	else if ( !strncmp(p,":|",2) && p[2]!=')' )
		switch_over( bgn_pipe_CB, p+1, p )
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
BMTraverseCBEnd

//---------------------------------------------------------------------------
//	fprint_output
//---------------------------------------------------------------------------
void
fprint_output( FILE *stream, char *expression, int level ) {
	char *p = expression+1;	// skip '>'
	if ( *p=='&' ) p++;
	if ( *p=='"' ) p = p_prune( PRUNE_IDENTIFIER, p );
	if ( *p=='\0' ) { fprintf( stream, "%s", expression ); return; }
	p++; // skip ':'
	for ( char *q=expression; q!=p; q++ ) fputc( *q, stream );
	fprint_expr( stream, p, level, OUTPUT ); }

