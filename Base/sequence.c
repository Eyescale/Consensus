#include <stdio.h>
#include <stdlib.h>

#include "pair.h"

#define DEBUG

//===========================================================================
//	segmentize
//===========================================================================
static void tie_up( listItem **, listItem **, listItem ** );

#define BASE_EXCEPTED \
	if (!(stack[ 1 ])) { p++; break; }

#define CLIP_SEQUENCE \
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
	build sequence:{
		| [ segment, NULL ]
		| alternative:[ :sequence, next:alternative ]
		| [ NULL, NULL ]
		}
*/
{
	listItem *sequence = NULL, *alternative = NULL;
	listItem *stack[ 2 ] = { NULL, NULL };
	char *p = expression;
	Pair *segment = newPair( p, NULL );
	while ( *p ) {
		switch ( *p ) {
		case '{':
			CLIP_SEQUENCE
			addItem( &stack[ 0 ], sequence );
			addItem( &stack[ 1 ], alternative );
			sequence = alternative = NULL;
			break;
		case ',':
			BASE_EXCEPTED
			CLIP_SEQUENCE
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
			BASE_EXCEPTED
			CLIP_SEQUENCE
			tie_up( &alternative, &sequence, stack );
			break;
		case ' ':
			BASE_EXCEPTED
			CLIP_SEQUENCE
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
				CLIP_SEQUENCE
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
	while (( stack[ 1 ] )) {
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
			*sequence = popListItem( &stack[ 0 ] );
			*sequence = addItem( sequence, *alternative );
		}
		else {
			listItem *s = popListItem( &stack[ 0 ] );
			*sequence = catListItem( *sequence, s );
		}
	}
	else {
		listItem *s = popListItem( &stack[ 0 ] );
		*sequence = catListItem( *sequence, s );
	}
	*alternative = popListItem( &stack[ 1 ] );
}

//===========================================================================
//	expand, firsti, nexti
//===========================================================================
#define PATH		0
#define BRANCH		1
#define SEQUENCE	2
void
expand( listItem *sequence, listItem **stack )
/*
	traverse sequence:{
		| [ segment, NULL ]
		| alternative:[ :sequence, next:alternative ]
		| [ NULL, NULL ]
		}
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
				addItem( &stack[ BRANCH ], newPair( item, item ));
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
				addItem( &stack[ BRANCH ], newPair( item, item ));
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
		Pair *index = i->ptr;
		next_i = i->next;
		listItem *alternative = index->value;
		if (( index->value = alternative->next ))
			break;
		else {
			freePair( index );
			popListItem( &stack[ BRANCH ] );
		}
	}
	if (!( stack[ BRANCH ] )) return 0;

	/* buid new path, setting branches in proper order
	*/
	for ( listItem *i=stack[ BRANCH ]; i!=NULL; i=i->next ) {
		Pair *index = i->ptr;
		addItem( &stack[ PATH ], ((listItem *) index->value )->ptr );
	}
	stack[ 3 ] = stack[ PATH ];
	return 1;
}

//===========================================================================
//	free_sequence
//===========================================================================
void
free_sequence( listItem *sequence )
/*
	free sequence:{
		| [ segment, NULL ]
		| alternative:[ :sequence, :alternative ]
		| [ NULL, NULL ]
		}
*/
{
	listItem *stack = NULL;
	listItem *i = sequence;
	for ( ; ; ) {
		Pair *item = i->ptr;
		if (( item->value )) {
			// item is a list of alternative sequences
			Pair *index = newPair( item, item );
			addItem( &stack, index );
			i = ((listItem *) item )->ptr;
			continue;
		}
		freePair( item->name );	// segment
		if (( i->next )) {
			i = i->next;
		}
		else if (( stack )) {
			for ( ; ; ) {
				Pair *index = stack->ptr;
				listItem *alternative = index->value;
				freeListItem((listItem **) &alternative->ptr );
				if (( alternative->next )) {
					index->value = alternative->next;
					i = alternative->next->ptr;
					break;
				}
				else {
					freePair( index );
					popListItem( &stack );
					if (!(stack)) goto RETURN;
				}
			}
		}
		else goto RETURN;
	}
RETURN:
	freeListItem( &sequence );
}

//===========================================================================
//	main	- DEBUG
//===========================================================================
#ifdef DEBUG
char *expression = "{ hello, hi }, { }{ \n\\0, world, { dear\\ ,, poor\\ , }{ you }}.\n";
// char *expression = "{ hello, hi }, { }{ \n\\0, world\n, { dear\\ ,, poor\\ , }{ you \n";
int
main( int argc, char *argv[] )
{
	printf( "expression:\n\t" );
	for ( char *p=expression; *p; p++ )
		if ( *p=='\n' ) printf( "\\n" ); else putchar( *p );
	printf( "\nresults:\n" );

	listItem *sequence = segmentize( expression );
	listItem *stack[ 4 ] = { NULL, NULL, NULL, NULL };
	do {
#if 1
		printf( "\t" );
		for ( listItem *i=firsti(sequence,stack); i!=NULL; i=nexti(i,stack) ) {
			Pair *item=i->ptr, *segment=item->name;
			if (( segment ))
				for ( char *p=segment->name; p!=segment->value; p++ )
					printf( "%c", *p );
			else break;
		}
#else
		expand( sequence, stack );
#endif
	}
	while ( cycle_through( stack ) );
	free_sequence( sequence );
}
#endif

