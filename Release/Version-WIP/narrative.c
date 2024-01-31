#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "narrative.h"
#include "expression.h"

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
int
narrative_output( FILE *stream, CNNarrative *narrative, int level ) {
	if ( narrative == NULL ) {
		fprintf( stderr, "Error: narrative_output: No narrative\n" );
		return 0; }
	char *proto = narrative->proto;
	if (( proto )) {
		if ( !is_separator( *proto ) ) fprintf( stream, ": " );
		fprintf( stream, "%s\n", proto ); }
	CNOccurrence *occurrence = narrative->root;
	listItem *i = newItem( occurrence ), *stack = NULL;
	for ( ; ; ) {
		occurrence = i->ptr;
		char *expression = occurrence->data->expression;
		int type = cast_i( occurrence->data->type );
		TAB( level );
		switch ( type ) {
		case ROOT: break;
		case ELSE: fprintf( stream, "else\n" ); break;
		default: if ( type&ELSE ) fprintf( stream, "else " );
			if ( type & PER ) fprintf( stream, "per " );
			else if ( type & IN ) fprintf( stream, "in " );
			else if ( type & (ON|ON_X) ) fprintf( stream, "on " );
			else if ( type & (DO|INPUT|OUTPUT) ) fprintf( stream, "do " );
			if ( type&DO ) fprint_expr( stream, expression, level );
			else fprintf( stream, "%s", expression );
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

