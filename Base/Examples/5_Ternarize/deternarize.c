#include <stdio.h>
#include <stdlib.h>

#include "list.h"
#include "pair.h"
#include "flags.h"
#include "string_util.h"
#include "ternarize.h"

//===========================================================================
//	deternarize
//===========================================================================
static void s_scan( CNString *, listItem * );

char *
deternarize( char *expression, BMTernaryCB pass_CB, void *user_data )
/*
	build Sequence:{
		| [ segment:Segment, NULL ]
		| [ NULL, sub:Sequence ]
		| [ NULL, NULL ] // ~.
		}
	where
		Segment:[ name, value ]	// a.k.a. { char *bgn, *end; } 
*/
{
	if ( !pass_CB ) return NULL;
	struct { listItem *sequence, *flags; } stack = { NULL, NULL };
	char *p = expression, *deternarized = NULL;
	int flags = 0, ternary = 0;
	listItem *sequence = NULL;
	Pair *segment = newPair( p, NULL );
	for ( int done=0; *p && !done; ) {
		switch ( *p ) {
		case ' ':
		case '\t':
			p++; break;
		case '(':
			/* Note: only pre-ternary-operated sequences are pushed
			   on stack.sequence
			*/
			if ( p_ternary( p ) ) {
				ternary = 1;
				// close current Sequence after '(' - without reordering,
				// which will take place on return
				segment->value = p+1;
				addItem( &sequence, newPair( segment, NULL ) );
				// push current Sequence on stack.sequence and start new
				addItem( &stack.sequence, sequence );
				sequence = NULL;
				segment = newPair ( p+1, NULL );
			}
			f_push( &stack.flags )
			f_reset( FIRST|LEVEL, 0 )
			p++; break;
		case '?':
			if is_f( INFORMED ) {
				if (!is_f( LEVEL )) { done=1; break; }
				// finish sequence==guard, reordered
				segment->value = p;
				addItem( &sequence, newPair( segment, NULL ) );
				reorderListItem( &sequence );
				// convert & evaluate guard
				CNString *s = newString();
				s_scan( s, sequence );
				char *guard = StringFinish( s, 0 );
				if ( pass_CB( guard, user_data ) ) {
					if ( p[1]==':' ) {
						// sequence==guard is our current candidate
						// segment is informed
						p = p_prune( PRUNE_TERNARY, p+1 ); // proceed to ")"
					}
					else {
						// release guard sequence
						free_ternarized( sequence );
						sequence = NULL;
						p++; // resume past "?"
						segment = newPair( p, NULL );
					}
				}
				else {
					// release guard sequence
					free_ternarized( sequence );
					sequence = NULL;
					// proceed to alternative
					p = p_prune( PRUNE_TERNARY, p ); // ":"
					if ( p[1]==':' || p[1]==')' ) {
						// ~. is our current candidate
						segment = NULL;
						p = p_prune( PRUNE_TERNARY, p ); // proceed to ")"
					}
					else {
						p++; // resume past ":"
						segment = newPair( p, NULL );
					}
				}
				freeString( s );
				f_set( TERNARY )
			}
			else { p++; f_set( INFORMED ) }
			break;
		case ':':
			if is_f( TERNARY ) {
				// option completed
				segment->value = p;
				// proceed to ")"
				p = p_prune( PRUNE_TERNARY, p );
			}
			else {	p++; f_clr( INFORMED ) }
			break;
		case ')':
			if (!is_f( LEVEL )) { done=1; break; }
			if is_f( TERNARY ) {
				// special cases: segment is ~. or completed option
				if ( !segment ) {
					// add as-is to on-going expression
					sequence = popListItem( &stack.sequence );
					addItem( &sequence, newPair( NULL, NULL ) );
				}
				else if ( !sequence ) {
					if ( !segment->value ) segment->value = p;
					// add as-is to on-going expression
					sequence = popListItem( &stack.sequence );
					addItem( &sequence, newPair( segment, NULL ) );
				}
				else {
					// finish current sequence, reordered
					if ( !segment->value ) segment->value = p;
					addItem( &sequence, newPair( segment, NULL ) );
					reorderListItem( &sequence );
					// add as sub-Sequence to on-going expression
					listItem *sub = sequence;
					sequence = popListItem( &stack.sequence );
					addItem( &sequence, newPair( NULL, sub ) );
				}
				segment = newPair( p, NULL );
			}
			f_pop( &stack.flags, 0 );
			f_set( INFORMED )
			p++; break;
		case ',':
			if (!is_f( LEVEL )) { done=1; break; }
			f_clr( INFORMED )
			p++; break;
		case '%':
			if ( p[1]=='(' ) {
				p++; break;
			}
			// no break
		case '*':
			if ( p[1]=='?' ) {
				f_set( INFORMED );
				p+=2; break;
			}
			// no break
		default:
			do p++; while ( !is_separator(*p) );
			f_set( INFORMED )
		}
	}
	if ( !ternary ) {
		freePair( segment );
		goto RETURN;
	}
	// finish current sequence, reordered
	if ( segment->name != p ) {
		segment->value = p;
		addItem( &sequence, newPair( segment, NULL ) );
	} else freePair( segment );
	reorderListItem( &sequence );

	// convert sequence to char *string
	CNString *s = newString();
	s_scan( s, sequence );
	deternarized = StringFinish( s, 0 );
	StringReset( s, CNStringMode );
	freeString( s );
RETURN:
	// release sequence and return string
	free_ternarized( sequence );
	if ((stack.sequence)||(stack.flags)) {
		fprintf( stderr, ">>>>> B%%: Error: deternarize: Memory Leak\n" );
		freeListItem( &stack.sequence );
		freeListItem( &stack.flags );
		free( deternarized );
		exit( -1 );
	}
	return deternarized;
}

//===========================================================================
//	s_scan, s_append, s_add_not_any
//===========================================================================
static void s_append( CNString *, char *bgn, char *end );
#define s_add( str ) \
	for ( char *p=str; *p; StringAppend(s,*p++) );

static void
s_scan( CNString *s, listItem *sequence )
/*
	scan ternary-operator guard
		Sequence:{
			| [ NULL, sub:Sequence ]
			| [ segment:Segment, NULL ]
			| [ NULL, NULL ] // ~.
			}
		where
			Segment:[ name, value ]	// a.k.a. { char *bgn, *end; } 
	into CNString
*/
{
	listItem *stack = NULL;
	listItem *i = sequence;
	for ( ; ; ) {
		Pair *item = i->ptr;
		if (( item->value )) {
			// item==[ NULL, sub:Sequence ]
			if (( i->next )) addItem( &stack, i->next );
			i = item->value; // parse sub-sequence
			continue;
		}
		else if (( item->name )) {
			// item==[ segment:Segment, NULL ]
			Pair *segment = item->name;
			s_append( s, segment->name, segment->value );
		}
		else s_add( "~." )
		// moving on
		if (( i->next ))
			i = i->next;
		else if (( stack ))
			i = popListItem( &stack );
		else break;
	}
}
static void
s_append( CNString *s, char *bgn, char *end )
{
	for ( char *p=bgn; p!=end; p++ )
		switch ( *p ) {
		case ' ':
		case '\t':
			break;
		default:
			StringAppend( s, *p );
		}
}