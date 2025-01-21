#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "narrative.h"
#include "expression.h"
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
		if ( type!=ROOT ) TAB( level );
		switch ( type ) {
		case ELSE: fprintf( stream, "else\n" );
		case ROOT: break;
		default:
			// output cmd
			if ( !(type&CASE) ) {
				if_( ELSE, "else " )
				if_( EN, "en " )
				else if_( PER, "per " )
				else if_( IN, "in " )
				else if_( ON|ON_X, "on " )
				else if_( DO|INPUT|OUTPUT, "do " ) }
			// output expr
			if ( type&DO )
				fprint_expr( stream, expression, level );
			else if ( type&OUTPUT )
				fprint_output( stream, expression, level );
			else	fprintf( stream, "%s", expression );
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

