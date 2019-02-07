#ifndef SEQUENCE_H
#define SEQUENCE_H

#include "macros.h"
#include "list.h"
#include "stream.h"

/*---------------------------------------------------------------------------
	Sequence Streaming Interface - non-recursive
---------------------------------------------------------------------------*/
/*
	A sequence is a list of pairs, where
	for each pair
		if pair->name is null, then
			pair->value holds an event value (character)
		else (if pair->name is not null) then
			pair->name is a string identifier
			pair->value is the new sequence
		endif
	end for
*/
typedef listItem Sequence;

#define CNSequenceValue( s )	((int)pair->value)
#define CNSequenceName( s )	s.name
#define CNSequenceNextName( s )	(pair->name)
#define CNNextSequence( s )	(pair->value)

#define CNOnOther	} else {
#define CNInOther	} else {

#define CNSequenceBegin( s, stream, list ) \
	struct { \
		char *name; \
		listItem *stack; \
	} s; \
	s.name = ""; \
	s.stack = NULL; \
	CNStreamBegin( void *, stream, list ) \
		CNOnStreamValue( stream, Pair *, pair ) \
			if ( 0 ) {
#define CNOnSequencePush( s ) \
			} else if (( pair->name )) {
#define CNInSequenceName( s, n ) \
				} else if ( !strcmp( s.name, n ) ) {
#define CNInSequenceNextName( s, n ) \
				} else if ( !strcmp( pair->name, n ) ) {
#define CNOnSequenceValue( s ) \
				addItem( &s.stack, s.name ); \
				s.name = pair->name; \
				CNStreamPush( stream, pair->value ); \
			} else {
#define CNInSequenceValue( s, e ) \
				} else if ( (int)pair->value == e ) {
#define CNOnSequencePop( s ) \
				CNStreamFlow( stream ); \
			} \
		CNOnStreamPop \
			CNInStreamPop( stream, Pair *, pair )
#define CNSequenceEnd( s ) \
				s.name = popListItem( &s.stack ); \
	CNStreamEnd( stream )


#endif	// SEQUENCE_H
