#include <stdio.h>
#include <stdlib.h>

#include "list.h"
#include "pair.h"
#include "flags.h"
#include "string_util.h"

listItem *ternarize( char *expression );
void free_ternarized( listItem *sequence );
void output_ternarized( listItem *sequence );

//===========================================================================
//	main
//===========================================================================
// char *expression = "( hello, world )";
// char *expression = "( hi:bambina ? hello : world )";
// char *expression = "( hi:bambina ?: carai )";
// char *expression = "chiquita:( hi:bambina ? caramba :):bambam, tortilla";
char *expression = "( hi ? hello ? You : world : caramba )";

int
main( int argc, char *argv[] )
{
	listItem *sequence = ternarize( expression );

	printf( "expression:\n\t" );
        for ( char *p=expression; *p; p++ )
		switch ( *p ) {
		case '\n':
			printf( "\\n" );
			break;
		default:
                	putchar( *p );
		}

        printf( "\nresults:\n" );
	output_ternarized( sequence );

	free_ternarized( sequence );
}

//===========================================================================
//	ternarize
//===========================================================================
#define	FIRST		1
#define	LEVEL		2
#define	SUB_EXPR	4
#define	INFORMED	8
#define	TERNARY		16
#define	FILTERED	32

#define FINISH( segment, sequence, p, reorder ) \
	if ( segment->name != p ) { \
		segment->value = p; \
		addItem( &sequence, newPair( segment, NULL ) ); \
		segment = newPair( p, NULL ); \
	} \
	if ( reorder ) reorderListItem( &sequence );

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
			// which will take place on ')'
			if is_f( SUB_EXPR ) p--;
			FINISH( segment, sequence, p, 0 );
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
				FINISH( segment, sequence, p, 1 );
				// My whole current Sequence is guard
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
				addItem( &stack.sequence, ternary );
				sequence = NULL;
				f_push( &stack.flags )
				f_clr( FIRST|INFORMED|FILTERED )
			}
			else {	f_set( INFORMED ) }
			p++; break;
		case ':':
			if is_f( TERNARY ) {
				FINISH( segment, sequence, p, 1 );
				// ternary==[ guard, [.,.] ] is on stack.sequence
				if is_f( FILTERED ) {
					Pair *ternary = popListItem( &stack.sequence );
					((Pair *) ternary->value )->value = sequence;
					sequence = NULL;
					f_pop( &stack.flags, 0 )
				}
				else {
					Pair *ternary = stack.sequence->ptr;
					((Pair *) ternary->value )->name = sequence;
					sequence = NULL;
					f_set( FILTERED )
				}
			}
			f_clr( INFORMED )
			p++; break;
		case ')':
			if (!is_f( LEVEL )) { done=1; break; }
			FINISH( segment, sequence, p, 1 );
			if is_f( TERNARY ) {
				// ternary==[ guard, [ passed, NULL ] ] is on stack.sequence
				Pair *ternary;
				do {
					ternary = popListItem( &stack.sequence );
					f_pop( &stack.flags, 0 );
				} while (!is_f( FIRST ));
				((Pair *) ternary->value )->value = sequence;
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
			if ( p[1]=='(' ) f_set( SUB_EXPR )
			// no break
		case '*':
			if ( p[1]=='?' ) p++;
			// no break
		default:
			p = p_prune( PRUNE_IDENTIFIER, p );
			f_set( INFORMED )
		}
	}
	// finish current Sequence
	FINISH( segment, sequence, p, 1 );
	freePair( segment );

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
		| NULL	// alternate
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
				/* item==[ guard:Sequence, p:[ passed:Sequence, alternative:Sequence ] ]
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
		freePair( item->name );	// segment
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

	SEE ALSO free_ternarized for comments in the code
*/
{
	listItem *stack = NULL;
	listItem *i = sequence;
	while (( i )) {
		Pair *item = i->ptr;
		if (( item->name )) {
			if (( item->value )) {
				// item==[ guard:Sequence, p:[ passed:Sequence, alternative:Sequence ] ]
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
		// output segment
		Pair *segment = item->name;
		printf( "\t" );
		for ( char *p=segment->name; p!=segment->value; p++ )
			putchar( *p );
		printf( "\n" );
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
