#include <stdio.h>
#include <stdlib.h>

#include "list.h"
#include "pair.h"

listItem * segmentize( char *expression );
void free_segmentized( listItem *sequence );
void expand( listItem *sequence, listItem **stack );
listItem * firsti( listItem *i, listItem **stack );
listItem * nexti( listItem *i, listItem **stack );
int cycle_through( listItem **stack );

//===========================================================================
//	main
//===========================================================================
char *expression = "{ hello, hi }, { }{ \n\\0, world, { dear\\ ,, poor\\ , }{ you }}.\n";
// char *expression = "{ hello, hi }, { }{ \n\\0, world\n, { dear\\ ,, poor\\ , }{ you \n";

int
main( int argc, char *argv[] )
{
	listItem *sequence = segmentize( expression );

	printf( "expression:\n\t" );
	for ( char *p=expression; *p; p++ )
		if ( *p=='\n' ) printf( "\\n" ); else putchar( *p );
	printf( "\nresults:\n" );
	listItem *stack[ 4 ] = { NULL, NULL, NULL, NULL };
	do {
#ifdef EXECUTIVE_SUMMARY
		expand( sequence, stack );
#else
		printf( "\t" );
		for ( listItem *i=firsti(sequence,stack); i!=NULL; i=nexti(i,stack) ) {
			Pair *item=i->ptr, *segment=item->name;
			if (( segment ))
				for ( char *p=segment->name; p!=segment->value; p++ )
					printf( "%c", *p );
			else break;
		}
#endif
	} while ( cycle_through( stack ) );

	free_segmentized( sequence );
}

//===========================================================================
//	segmentize
//===========================================================================
static void tie_up( listItem **, listItem **, listItem ** );

#define SEQUENCE	0
#define ALTERNATIVE	1
#define BRANCH		1
#define PATH		2

#define CLIP( sequence, segment, p ) \
	if ( segment->name == p ) { \
		p++; segment->name = p; \
	} else { \
		segment->value = p++; \
		addItem( &sequence, newPair( segment, NULL ) ); \
		segment = newPair( p, NULL ); \
	}

listItem *
segmentize( char *expression )
/*
	build Sequence:{
		| [ segment:Segment, NULL ]
		| Alternative:[ :Sequence, next:Alternative ]
		| [ NULL, NULL ]
		}
	where
		Segment:[ name, value ]	// a.k.a. { char *bgn, *end; }
*/
{
	listItem *sequence = NULL, *alternative = NULL;
	listItem *stack[ 2 ] = { NULL, NULL };
	char *p = expression;
	Pair *segment = newPair( p, NULL );
	while ( *p ) {
		switch ( *p ) {
		case '{':
			CLIP( sequence, segment, p );
			addItem( &stack[ SEQUENCE ], sequence );
			addItem( &stack[ ALTERNATIVE ], alternative );
			sequence = alternative = NULL;
			break;
		case ',':
			if ( stack[ ALTERNATIVE ]==NULL ) { p++; break; }
			CLIP( sequence, segment, p );
			if (( sequence )) {
				reorderListItem( &sequence );
				alternative = addItem( &alternative, sequence );
				sequence = NULL;
			}
			if ( *p == ',' ) {
				segment->value = p++;
				addItem( &sequence, newPair( segment, NULL ) );
				segment = newPair( p, NULL );
				alternative = addItem( &alternative, sequence );
				sequence = NULL;
			}
			break;
		case '}':
			if ( stack[ ALTERNATIVE ]==NULL ) { p++; break; }
			CLIP( sequence, segment, p );
			tie_up( &alternative, &sequence, stack );
			break;
		case ' ':
			if ( stack[ ALTERNATIVE ]==NULL ) { p++; break; }
			CLIP( sequence, segment, p );
			break;
		case '\\':
			switch ( p[1] ) {
			case '0':
				segment->value = p++;
				addItem( &sequence, newPair( segment, NULL ) );
				addItem( &sequence, newPair( NULL, NULL ) );
				segment = newPair( p, NULL );
				break;
			case '{':
			case ',':
			case '}':
			case '\\':
			case ' ':
				CLIP( sequence, segment, p );
				break;
			default:
				p++;
			}
			// no break
		default:
			p++;
		}
	}
	segment->value = p;
	addItem( &sequence, newPair( segment, NULL ) );
	while (( stack[ ALTERNATIVE ] )) {
		tie_up( &alternative, &sequence, stack );
	}
	reorderListItem( &sequence );
	return sequence;
}
static void
tie_up( listItem **alternative, listItem **sequence, listItem **stack )
{
	if (( *alternative )) {
		if (( *sequence )) {
			reorderListItem( sequence );
			addItem( alternative, *sequence );
		}
		if (( (*alternative)->next )) {
			reorderListItem( alternative );
			*sequence = popListItem( &stack[ SEQUENCE ] );
			*sequence = addItem( sequence, *alternative );
		}
		else {
			listItem *s = popListItem( &stack[ SEQUENCE ] );
			*sequence = catListItem( *sequence, s );
		}
	}
	else {
		listItem *s = popListItem( &stack[ SEQUENCE ] );
		*sequence = catListItem( *sequence, s );
	}
	*alternative = popListItem( &stack[ ALTERNATIVE ] );
}

//===========================================================================
//	expand, firsti, nexti
//===========================================================================
void
expand( listItem *sequence, listItem **stack )
/*
	traverse Sequence:{
		| [ segment:Segment, NULL ]
		| Alternative:[ :Sequence, next:Alternative ]
		| [ NULL, NULL ]
		}
	where
		Segment:[ name, value ]	// a.k.a. { char *bgn, *end; }
*/
{
	listItem *i=sequence, *j=stack[ PATH ];
	for ( ; ; ) {
		Pair *item = i->ptr;
		if (( item->value )) {
			// item is a list of alternative sequences
			addItem( &stack[ SEQUENCE ], i );
			if (( j )) {
				i = j->ptr;
				j = j->next;
			}
			else {
				addItem( &stack[ BRANCH ], newPair( NULL, item ));
				i = ((listItem *) item )->ptr;
			}
			continue;
		}
		Pair *segment = item->name;
		if (( segment )) {
			for ( char *p=segment->name; p!=segment->value; p++ )
				printf( "%c", *p );
		}
		else return;
		for ( ; ; ) {
			i = i->next;
			if (( i )) break;
			else if (( stack[ SEQUENCE ] ))
				i = popListItem( &stack[ SEQUENCE ] );
			else return;
		}
	}
}
listItem *
firsti( listItem *i, listItem **stack )
{
	for ( ; ; ) {
		Pair *item = i->ptr;
		if (( item->value )) {
			// item is a list of alternative sequences
			addItem( &stack[ SEQUENCE ], i );
			if (( stack[ 3 ] )) {
				i = stack[ 3 ]->ptr;
				stack[ 3 ] = stack[ 3 ]->next;
			}
			else {
				addItem( &stack[ BRANCH ], newPair( NULL, item ));
				i = ((listItem *) item )->ptr;
			}
			continue;
		}
		return i;
	}
}
listItem *
nexti( listItem *i, listItem **stack )
{
	for ( ; ; ) {
		i = i->next;
		if (( i )) break;
		else if (( stack[ SEQUENCE ] ))
			i = popListItem( &stack[ SEQUENCE ] );
		else return NULL;
	}
	return firsti( i, stack );
}

//===========================================================================
//	cycle_through
//===========================================================================
int
cycle_through( listItem **stack )
{
	freeListItem( &stack[ SEQUENCE ] );
	freeListItem( &stack[ PATH ] );
	if (!( stack[ BRANCH ] )) return 0;

	/* update branches to the next alternative combo
	*/
	listItem *next_i;
	for ( listItem *i=stack[ BRANCH ]; i!=NULL; i=next_i ) {
		Pair *iterator = i->ptr;
		next_i = i->next;
		listItem *alternative = iterator->value;
		if (( iterator->value = alternative->next ))
			break;
		else {
			freePair( iterator );
			popListItem( &stack[ BRANCH ] );
		}
	}
	if (!( stack[ BRANCH ] )) return 0;

	/* buid new path, setting branches in proper order
	*/
	for ( listItem *i=stack[ BRANCH ]; i!=NULL; i=i->next ) {
		Pair *iterator = i->ptr;
		addItem( &stack[ PATH ], ((listItem *) iterator->value )->ptr );
	}
	stack[ 3 ] = stack[ PATH ];
	return 1;
}

//===========================================================================
//	free_segmentized
//===========================================================================
void
free_segmentized( listItem *sequence )
/*
	free Sequence:{
		| [ segment:Segment, NULL ]
		| Alternative:[ :Sequence, next:Alternative ]
		| [ NULL, NULL ]
		}
	where
		Segment:[ name, value ]	// a.k.a. { char *bgn, *end; }
*/
{
	listItem *stack = NULL;
	listItem *i = sequence;
	while (( i )) {
		Pair *item = i->ptr;
		if (( item->value )) {
			// item is a list of alternative sequences
			listItem *j = (listItem *) item;
			addItem( &stack, newPair( j, i->next ) );
			i = j->ptr;
			continue;
		}
		if (( item->name )) freePair( item->name ); // segment
		freePair( item );
		if (( i->next ))
			i = i->next;
		else if (( stack )) {
			do {
				item = stack->ptr;
				listItem *alternative = item->name;
				freeListItem((listItem **) &alternative->ptr );
				if (( alternative->next )) {
					item->name = alternative->next;
					freeItem( alternative );
					alternative = item->name;
					i = alternative->ptr;
					break;
				}
				else {
					i = item->value;
					freeItem( alternative );
					freePair( item );
					popListItem( &stack );
				}
			} while ( !i && (stack) );
		}
		else break;
	}
	freeListItem( &sequence );
}

