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
static void s_scang( CNString *, listItem * );
static void s_append( CNString *, int, char *bgn, char *end );
#define s_add( str ) \
	for ( char *p=str; *p; StringAppend(s,*p++) );

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
	listItem *sequence = NULL;
	int flags = 0, ternary = 0;
	char *p = expression;
	Pair *segment = newPair( p, NULL );
	for ( int done=0; *p && !done; ) {
		switch ( *p ) {
		case ' ':
		case '\t':
			p++; break;
		case '(':
			/* Note: only ternary-operated sequences are pushed
			   on stack.sequence
			*/
			if ( p_ternary( p ) ) {
				ternary = 1;
				// finish current Sequence - without reordering,
				// which will take place on ')' or '?'
				if is_f( SUB_EXPR ) p--;
				if ( segment->name != p ) {
					segment->value = p;
					addItem( &sequence, newPair( segment, NULL ) );
					segment = newPair ( p, NULL );
				}
				if is_f( SUB_EXPR ) { p++; f_clr( SUB_EXPR ) }
				// start a new Sequence
				addItem( &stack.sequence, sequence );
				sequence = NULL;
			}
			f_push( &stack.flags )
			f_reset( FIRST|LEVEL, 0 )
			p++; break;
		case '?':
			if is_f( INFORMED ) {
				if (!is_f( LEVEL )) { done=1; break; }
				/* sequence==guard==either
					{[%(,_[, ..., [_,?[}
				    or	{[(,_[,  ..., [_,?[} 
				    or	{[_,_[,  ..., [_,?[}  N-ary case
				*/
				// finish sequence, reordered
				segment->value = p;
				addItem( &sequence, segment );
				reorderListItem( &sequence );
				// convert & evaluate guard
				CNString *s = newString();
				s_scang( s, sequence );
				char *guard = StringFinish( s, 0 );
				if ( pass_CB( guard, user_data ) ) {
					if ( p[1]==':' ) {
						// sequence is our current candidate
						// proceed to ")"
						p = p_prune( PRUNE_TERNARY, p+1 );
					}
					else {
						// release guard sequence
						free_ternarized( sequence );
						sequence = NULL;
						// resume past "?"
						segment = newPair( p+1, NULL );
					}
				}
				else {
					// proceed to alternative
					p = p_prune( PRUNE_TERNARY, p ); // ":"
					if ( p[1]==':' || p[1]==')' ) {
						// ~. is our current candidate
						segment = NULL;
						// release guard sequence
						free_ternarized( sequence );
						sequence = NULL;
						// proceed to ")"
						p = p_prune( PRUNE_TERNARY, p );
					}
					else {
						// release guard sequence
						free_ternarized( sequence );
						sequence = NULL;
						// resume past ":"
						segment = newPair( p+1, NULL );
					}
				}
				freeString( s );
				f_set( TERNARY )
			}
			else {	f_set( INFORMED ) }
			p++; break;
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
				// specical cases: segment is ~. or completed option
				if ( segment==NULL || (segment->value) ) {
					// add as-is to on-going expression
					sequence = popListItem( &stack.sequence );
					addItem( &sequence, newPair( segment, NULL ) );
				}
				else {
					// finish current sequence, reordered
					segment->value = p;
					addItem( &sequence, newPair( segment, NULL ) );
					reorderListItem( &sequence );
					// resume on-going expression
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
				f_set( SUB_EXPR )
				p++; break;
			}
			// no break
		case '*':
			if ( p[1]=='?' ) p++;
			// no break
		default:
			do p++; while ( !is_separator(*p) );
			f_set( INFORMED )
		}
	}
	if ( !ternary ) {
		freePair( segment );
		free_ternarized( sequence );
		return NULL;
	}
	// finish current sequence, reordered
	if ( segment->name != p ) {
		segment->value = p;
		addItem( &sequence, newPair( segment, NULL ) );
	} else freePair( segment );
	reorderListItem( &sequence );

	// convert sequence to char *string
	CNString *s = newString();
	for ( listItem *i=sequence; i!=NULL; i=i->next ) {
		Pair *item = i->ptr;
		if (( item->value )) {
			// item==[ NULL, sub:Sequence ]
			s_scang( s, item->value );
		}
		else if (( item->name )) {
			// item==[ segment:Segment, NULL ]
			Pair *segment = item->name;
			s_append( s, 0, segment->name, segment->value );
		}
		else {	s_add( "~." ) }
	}
	char *deternarized = StringFinish( s, 0 );
	StringReset( s, CNStringMode );
	freeString( s );

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
//	s_scang
//===========================================================================
static void s_start( CNString *, char *bgn, char *end );

static void
s_scang( CNString *s, listItem *sequence )
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
	int first = 1;
	for ( ; ; ) {
		Pair *item = i->ptr;
		if ( first ) {
			/* first item in Sequence is always [ segment:Segment, NULL ]
			   where segment can be either
			  	[ %(, _ [
			  	[ (, _ [
			  	[ _, _ [ // N-ary case
			*/
			Pair *segment = item->name;
			s_start( s, segment->name, segment->value );
			first = 0;
		}
		else if (( item->value )) {
			// item==[ NULL, sub:Sequence ]
			if (( i->next )) addItem( &stack, i->next );
			i = item->value; // parse sub-sequence
			first = 1;
			continue;
		}
		else if (( item->name )) {
			// item==[ segment:Segment, NULL ]
			Pair *segment = item->name;
			s_append( s, 1, segment->name, segment->value );
		}
		else {	s_add( "~." ) }
		// moving on
		if (( i->next ))
			i = i->next;
		else if (( stack ))
			i = popListItem( &stack );
		else break;
	}
}
static void
s_start( CNString *s, char *bgn, char *end )
{
	if ( bgn[ 0 ]=='%' && bgn[ 1 ]=='(' ) {
		s_add( "(" )
		bgn += 2;
	}
	else if ( bgn[ 0 ]=='(' ) {
		s_add( "(?:" )
		bgn++;
	}
	else {	s_add( "(" ) }
	s_append( s, 1, bgn, end );
}
static void
s_append( CNString *s, int guard, char *bgn, char *end )
/*
	skip blank characters, and, if guard, replace
	'?' at the end of segment with ')'
*/
{
	for ( char *p=bgn; p!=end; p++ ) {
		switch ( *p ) {
		case ' ':
		case '\t':
			break;
		default:
			StringAppend( s, *p );
		}
	}
	if ( guard && *end=='?' )
		StringAppend( s, ')' );
}
