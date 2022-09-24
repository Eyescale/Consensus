#include <stdio.h>
#include <stdlib.h>

#include "list.h"
#include "pair.h"
#include "flags.h"
#include "string_util.h"
#include "ternarize.h"

//===========================================================================
//	ternarize
//===========================================================================
listItem *
ternarize( char *expression )
/*
	build Sequence:{
		| [ segment:Segment, NULL ]
		| [ NULL, sub:Sequence ]
		| [ guard:Sequence, [ passed:Sequence, alternate:Sequence ] ]
		| NULL	// alternate
		}
	where
		Segment:[ name, value ]	// a.k.a. { char *bgn, *end; } 
*/
{
	struct { listItem *sequence, *flags; } stack = { NULL, NULL };
	listItem *sequence = NULL;
	int flags = 0;
	char *p = expression;
	Pair *segment = newPair( p, NULL );
	for ( int done=0; *p && !done; ) {
		switch ( *p ) {
		case ' ':
		case '\t':
			p++; break;
		case '(':
			// finish current Sequence - without reordering,
			// which will take place on '?' or ')'
			if is_f( SUB_EXPR ) p--;
			if ( segment->name != p ) {
				segment->value = p;
				addItem( &sequence, newPair( segment, NULL ) );
				segment = newPair( p, NULL );
			}
			if is_f( SUB_EXPR ) { p++; f_clr( SUB_EXPR ) }
			// start a new Sequence
			addItem( &stack.sequence, sequence );
			sequence = NULL;
			f_push( &stack.flags )
			f_reset( FIRST|LEVEL, 0 )
			p++; break;
		case '?':
			if is_f( INFORMED ) {
				if (!is_f( LEVEL )) { done=1; break; }
				// finish current Sequence - reordered
				if ( segment->name != p ) { // should not be, here
					segment->value = p;
					addItem( &sequence, newPair( segment, NULL ) );
					segment = newPair( p, NULL );
				}
				reorderListItem( &sequence );
				// whole Sequence is guard
				Pair *ternary = newPair( sequence, newPair( NULL, NULL ) );
				if is_f( TERNARY ) {
					// sup==[ guard, [.,.] ] is on stack.sequence
					Pair *sup = stack.sequence->ptr;
					if is_f( FILTERED )
						((Pair *) sup->value )->value = newItem( ternary );
					else
                                                ((Pair *) sup->value )->name = newItem( ternary );
				}
				else { f_set( TERNARY ) }
				// start new sequence
				addItem( &stack.sequence, ternary );
				sequence = NULL;
				f_push( &stack.flags )
				f_clr( FIRST|INFORMED|FILTERED )
			}
			else {	f_set( INFORMED ) }
			p++; break;
		case ':':
			if is_f( TERNARY ) {
				// finish current sequence, reordered
				if ( segment->name != p ) {
					segment->value = p;
					addItem( &sequence, newPair( segment, NULL ) );
					segment = newPair( p, NULL );
				}
				reorderListItem( &sequence );
				// ternary==[ guard, [.,.] ] is on stack.sequence
				if is_f( FILTERED ) {
					Pair *ternary = popListItem( &stack.sequence );
					((Pair *) ternary->value )->value = sequence;
					f_pop( &stack.flags, 0 )
				}
				else {
					Pair *ternary = stack.sequence->ptr;
					((Pair *) ternary->value )->name = sequence;
					f_set( FILTERED )
				}
				sequence = NULL;
			}
			f_clr( INFORMED )
			p++; break;
		case ')':
			if (!is_f( LEVEL )) { done=1; break; }
			// finish current sequence, reordered
			if ( segment->name != p ) {
				segment->value = p;
				addItem( &sequence, newPair( segment, NULL ) );
				segment = newPair( p, NULL );
			}
			reorderListItem( &sequence );
			if is_f( TERNARY ) {
				// ternary==[ guard, [ passed, NULL ] ] is on stack.sequence
				Pair *ternary = popListItem( &stack.sequence );
				f_pop( &stack.flags, 0 );
				((Pair *) ternary->value )->value = sequence;
				while (!is_f( FIRST )) {
					ternary = popListItem( &stack.sequence );
					f_pop( &stack.flags, 0 );
				}
				sequence = popListItem( &stack.sequence );
				addItem( &sequence, ternary );
			}
			else {
				listItem *sub = sequence;
				sequence = popListItem( &stack.sequence );
				addItem( &sequence, newPair( NULL, sub ) );
			}
			f_pop( &stack.flags, 0 );
			f_set( INFORMED );
			p++; break;
		case ',':
			if (!is_f( LEVEL )) { done=1; break; }
			f_clr( INFORMED )
			p++; break;
		case '%':
			if ( p[1]=='(' ) {
				f_set( SUB_EXPR )
				p++; break;
			}
			// no break;
		case '*':
			if ( p[1]=='?' ) {
				f_set( INFORMED )
				p+=2; break;
			}
			// no break
		default:
			do p++; while ( !is_separator(*p) );
			f_set( INFORMED )
		}
	}
	// finish current sequence, reordered
	if ( segment->name != p ) {
		segment->value = p;
		addItem( &sequence, newPair( segment, NULL ) );
	}
	else freePair( segment );
	reorderListItem( &sequence );

	if ((stack.sequence)||(stack.flags)) {
		fprintf( stderr, ">>>>> B%%: Error: ternarize: Memory Leak\n" );
		freeListItem( &stack.sequence );
		freeListItem( &stack.flags );
		exit( -1 );
	}
	return sequence;
}

//===========================================================================
//	free_ternarized
//===========================================================================
void
free_ternarized( listItem *sequence )
/*
	free Sequence:{
		| [ segment:Segment, NULL ]
		| [ NULL, sub:Sequence ]
		| [ guard:Sequence, [ passed:Sequence, alternate:Sequence ] ]
		| [ NULL, NULL ] // ~.
		}
	where
		Segment:[ name, value ]	// a.k.a. { char *bgn, *end; } 
*/
{
	listItem *stack = NULL;
	listItem *i = sequence;
	while (( i )) {
		Pair *item = i->ptr;
		if (( item->name )) {
			if (( item->value )) {
				/* item==[ guard:Sequence, p:[ passed:Sequence, alternate:Sequence ] ]
				   We want the following on the stack
				   if (( p->value ))
					[ p->value, i->next ]
					[ p->name, p->value ]
					[ item->name, p->name ]
				   otherwise
					[ p->name, i->next ]
					[ item->name, p->name ]
				   as we start traversing item->name==guard
				*/
				Pair *p = item->value;
				if (( p->value ))
					addItem( &stack, newPair( p->value, i->next ) );
				else p->value = i->next;
				addItem( &stack, p );
				item->value = p->name;
				addItem( &stack, item );
				i = item->name;	// traverse guard
				continue;
			}
		}
		else if (( item->value )) {
			/* item==[ NULL, sub:Sequence ]
			   We want the following on the stack:
			  	[ item->value, i->next ]
			   as we start traversing item->value==sub
			*/
			item->name = item->value;
			item->value = i->next;
			addItem( &stack, item );
			i = item->name; // traverse sub
			continue;
		}
		if (( item->name )) freePair( item->name ); // segment
		freePair( item );
		if (( i->next ))
			i = i->next;
		else if (( stack )) {
			do {
				item = popListItem( &stack );
				// free previous Sequence
				freeListItem((listItem**) &item->name );
				// start new one
				i = item->value;
				freePair( item );
			}
			while ( !i && (stack) );
		}
		else break;
	}
	freeListItem( &sequence );
}

//===========================================================================
//	output_ternarized
//===========================================================================
#define TAB_BASE	0
#define TAB_OFFSET	1
static void output_s( int *tab, char *bgn, char *end );

void
output_ternarized( listItem *sequence )
/*
	output Sequence:{
		| [ segment:Segment, NULL ]
		| [ NULL, sub:Sequence ]
		| [ guard:Sequence, [ passed:Sequence, alternate:Sequence ] ]
		| NULL	// alternate
		}
	where
		Segment:[ name, value ]	// a.k.a. { char *bgn, *end; } 

	SEE ALSO free_ternarized for more comments in the code
*/
{
	listItem *stack = NULL;
	listItem *i = sequence;
	int tab[ 2 ] = { 1, 0 };
	for ( ; ; ) {
		Pair *item = i->ptr;
		if (( item->name )) {
			if (( item->value )) {
				// item==[ guard:Sequence, p:[ passed:Sequence, alternate:Sequence ] ]
				Pair *p = item->value;
				if (( i->next )) addItem( &stack, i->next );
				if (( p->value )) addItem( &stack, p->value );
				addItem( &stack, p->name );
				i = item->name;	// traverse guard
				continue;
			}
		}
		else if (( item->value )) {
			// item==[ NULL, sub:Sequence ]
			if (( i->next )) addItem( &stack, i->next );
			i = item->value; // traverse sub
			continue;
		}
		if (( item->name )) {
			// item==[ segment:Segment, NULL ]
			Pair *segment = item->name;
			output_s( tab, segment->name, segment->value );
		}
		// moving on
		if (( i->next )) {
			i = i->next;
		}
		else if (( stack )) {
			i = popListItem( &stack );
		}
		else break;
	}
}
static int tab_bgn( int *tab, char *bgn );
static void tab_end( int *tab, char *bgn, char *end );
static void
output_s( int *tab, char *bgn, char *end )
{
	int tabs = tab_bgn( tab, bgn );
	while ( tabs-- ) putchar( '\t' );
	for ( char *p=bgn; p!=end; p++ )
		putchar( *p );
	tab_end( tab, bgn, end );
	putchar( '\n' );
}
static int
tab_bgn( int *tab, char *bgn )
{
	switch ( *bgn ) {
	case '?':
		tab[ TAB_OFFSET ]++;
		break;
	case ')':
		tab[ TAB_BASE ]--;
		break;
	}
	return tab[ TAB_OFFSET ] > 0 ?
		tab[ TAB_BASE ] + tab[ TAB_OFFSET ] :
		tab[ TAB_BASE ];
}
static void
tab_end( int *tab, char *bgn, char *end )
{
	switch ( *bgn ) {
	case '%':
		if ( bgn[1]!='(' )
			break;
	case '(':
		tab[ TAB_BASE ]++;
		tab[ TAB_OFFSET ] = -1;
		break;
	case ':':
		tab[ TAB_OFFSET ]--;
		break;
	}
}
