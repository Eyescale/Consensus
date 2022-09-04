#ifndef MACROS_H
#define MACROS_H

// #define DEBUG

//===========================================================================
//	Parser event processing utilities - macros
//===========================================================================
#define	CNCaughtEvent	1
#define	CNCaughtState	2
#define	CNCaughtTrans	4

#define BEGIN		int caught; \
			do { \
				caught = CNCaughtEvent;

#define bgn_				if ( 0 ) {
#define in_( s )			} else if ( !strcmp( state, (s) ) ) { \
						caught |= CNCaughtState;
#define in_other			} else { \
						caught |= CNCaughtState;
#define in_any				} else { \
						caught |= CNCaughtState;
#define in_none_sofar			} else if (!(caught & CNCaughtState)) { \
						caught |= CNCaughtState;
#define on_( e )			} else if ( event==(e) ) {
#define on_space			} else if ( event==' ' || event=='\t' ) {
#define on_separator			} else if ( is_separator( event ) ) {
#define on_printable			} else if ( is_printable( event ) ) {
#define on_escapable			} else if ( is_escapable( event ) ) {
#define on_xdigit			} else if ( is_xdigit( event ) ) {
#define on_digit			} else if ( isdigit( event ) ) {
#define ons( s )			} else if ( strmatch( s, event ) ) {
#define on_other			} else {
#define on_any				} else {
#define same					state
#define do_( next )				state = (next); \
						caught |= CNCaughtTrans;
#define REENTER					caught &= ~CNCaughtEvent;
#define end				}

#define END				end \
			} while (!( caught & CNCaughtEvent ));

#endif	// MACROS_H
