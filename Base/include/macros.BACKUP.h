#ifndef MACROS_H
#define MACROS_H

// #define DEBUG

//===========================================================================
//	Parser event processing utilities - macros
//===========================================================================
#define	CNCaughtEvent	1
#define	CNCaughtState	2
#define	CNCaughtTrans	4
#define	CNCaughtAgain	8
#define	CNCaughtTest	16

#define BEGIN		int caught; \
			do { \
				caught = 0;

#define bgn_				if ( 0 ) {
#define in_( s )			} else if ( !strcmp( state, (s) ) ) { \
						caught |= CNCaughtState;
#define in_other			} else { \
						caught |= CNCaughtState;
#define in_any				} else { \
						caught |= CNCaughtState;
#define in_none_sofar			} else if (!(caught & CNCaughtState)) { \
						caught |= CNCaughtState;
#define on_( e )			} else if ( event==(e) ) { \
						caught |= CNCaughtEvent;
#define on_space			} else if ( event==' ' || event=='\t' ) { \
						caught |= CNCaughtEvent;
#define on_separator			} else if ( is_separator( event ) ) { \
						caught |= CNCaughtEvent;
#define on_printable			} else if ( is_printable( event ) ) { \
						caught |= CNCaughtEvent;
#define on_escapable			} else if ( is_escapable( event ) ) { \
						caught |= CNCaughtEvent;
#define on_xdigit			} else if ( is_xdigit( event ) ) { \
						caught |= CNCaughtEvent;
#define on_digit			} else if ( isdigit( event ) ) { \
						caught |= CNCaughtEvent;
#define ons( s )			} else if ( strmatch( s, event ) ) { \
						caught |= CNCaughtEvent;
#define on_other			} else { \
						caught |= CNCaughtEvent;
#define on_any				} else { \
						caught |= CNCaughtEvent;
#define same					state
#define do_( next )				state = (next); \
						caught |= CNCaughtTrans;
#define REENTER					caught |= CNCaughtAgain;
#define end				}

#define END				end \
			} while (!( caught&CNCaughtAgain ));

#endif	// MACROS_H
