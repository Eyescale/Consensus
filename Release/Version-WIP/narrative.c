#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "narrative.h"
#include "expression.h"
#include "fprint_expr.h"
#include "errout.h"

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

static inline void
fprint_switch( FILE *stream, char *expression, int level, int type ) {
	if ( type & DO )
		fprint_expr( stream, expression, level );
	else if ( type & OUTPUT )
		fprint_output( stream, expression, level );
	else fprintf( stream, "%s", expression );
	fprintf( stream, "\n" ); }

//---------------------------------------------------------------------------
//	fprint_expr
//---------------------------------------------------------------------------
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

//---------------------------------------------------------------------------
//	fprint_output
//---------------------------------------------------------------------------
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

