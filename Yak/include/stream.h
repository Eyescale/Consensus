#ifndef STREAM_H
#define STREAM_H

#include "macros.h"

/*---------------------------------------------------------------------------
	String Streaming Interface - non-recursive
---------------------------------------------------------------------------*/
#define CNStringStreamBegin( str, p, base, status ) \
	p = str; \
	int status = 0;	\
	for ( char *state=base; !status; p++ ) { \
		int event = *p; \
		BEGIN; bgn_
#define CNStringStreamEnd \
		END \
	}

/*---------------------------------------------------------------------------
	List Streaming Interface - non-recursive
---------------------------------------------------------------------------*/
#define CNStreamData( s )	s.retour
#define CNStreamRetour( s )	s.data->ptr

#define CNStreamBegin( type, s, list ) \
	struct { \
		listItem *data; \
		listItem *stack; \
		listItem *i; \
		type retour; \
	} s; \
	s.data = NULL; \
	s.stack = NULL; \
	s.retour = NULL; \
	s.i = list; \
	do { \
		if (( s.stack )) { \
			s.retour = popListItem( &s.data ); \
			s.i = popListItem( &s.stack ); \
			s.i = s.i->next; \
		}
#define CNOnStreamValue( s, type, variable ) \
		while (( s.i )) { \
			type variable = s.i->ptr;
#define CNStreamPush( s, a ) { \
			addItem( &s.data, s.retour ); \
			addItem( &s.stack, s.i ); \
			s.retour = NULL; \
			s.i = a; }
#define CNStreamFlow( s ) \
			s.i = s.i->next;
#define CNOnStreamPop \
		} {
#define CNInStreamPop( s, type, variable ) \
		} if (( s.stack )) { \
			type variable = ((listItem *) s.stack->ptr )->ptr;
#define CNStreamEnd( s ) \
		} \
	} while (( s.stack ));

#define CNStreamReturn( s, retval ) \
	freeListItem( &s.data ); \
	freeListItem( &s.stack ); \
	return retval;

/*---------------------------------------------------------------------------
	Nested List Streaming Interface - non-recursive
---------------------------------------------------------------------------*/
#define CNOuterStreamBegin( type, s1, list ) \
	struct { \
		listItem *data; \
		listItem *stack; \
		listItem *i; \
		type retour; \
		listItem *ref; \
		listItem *pushed; \
		listItem *s2_i; \
	} s1; \
	s1.data = NULL; \
	s1.stack = NULL; \
	s1.retour = NULL; \
	s1.i = list; \
	s1.ref = s1.i; \
	s1.pushed = NULL; \
	s1.s2_i = NULL; \
	do { \
		if (( s1.stack ) || ( s1.pushed )) { \
			s1.retour = popListItem( &s1.data ); \
			if ( s1.i == s1.ref ) { \
				if ( s1.pushed ) { \
					s1.i = popListItem( &s1.pushed ); \
					s1.i = s1.i->next; \
					s1.ref = NULL; \
				} \
				else { \
					s1.i = popListItem( &s1.stack ); \
					s1.i = s1.i->next; \
					s1.ref = s1.i; \
				} \
			} \
			else s1.i = s1.ref; \
		}
#define CNOnOuterStreamValue( s1, type, variable ) \
		while (( s1.i )) { \
			type variable = s1.i->ptr;
#define CNOuterInnerStreamBegin( type, s1, s2, list ) \
			struct { listItem *i; } s2; \
			if ( s1.i == s1.ref ) \
				s2.i = list; \
			else { \
				s1.ref = s1.i; \
				s2.i = popListItem( &s1.s2_i ); \
				s2.i = s2.i->next; \
				fprintf( stderr, "NEXT: %0x\n", (int) s2.i ); \
			}
#define CNOnInnerStreamValue( s2, type, variable ) \
			while (( s2.i ) && ( s1.i )) { \
				type variable = s2.i->ptr;
#define CNInnerOuterStreamPush( s2, s1, list ) \
				addItem( &s1.data, s1.retour ); \
				s1.retour = NULL; \
				s1.i = NULL; \
				s1.ref = list; \
				addItem( &s1.pushed, s1.ref ); \
				addItem( &s1.s2_i, s2.i );
#define CNInnerStreamFlow( s2 ) \
				s2.i = s2.i->next;
#define CNInnerStreamEnd( s2 ) \
			}
#define CNOuterStreamFlow( s1 ) \
			if (( s1.i )) s1.ref = s1.i = s1.i->next;
#define CNOuterStreamPush( s1, list ) \
			if (( s1.i )) { \
				addItem( &s1.data, s1.retour ); \
				addItem( &s1.stack, s1.i ); \
				s1.retour = NULL; \
				s1.i = list; \
			}
#define CNOnStreamPop \
		} {
#define CNInStreamPop( s, type, variable ) \
		} if (( s.stack )) { \
			type variable = ((listItem *) s.stack->ptr )->ptr;
#define CNOuterStreamEnd( s1 ) \
		} \
	} while ((s1.stack) || (s1.pushed));



#endif	// STREAM_H
