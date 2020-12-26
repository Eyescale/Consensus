#ifndef MACROS_H
#define MACROS_H

#define BEGIN		struct { int event, state, transition; } caught; \
			do { \
				caught.transition = 0; \
				caught.state = 0; \
				caught.event = 1;

#define bgn_				if ( 0 ) {
#define in_( s )			} else if ( !strcmp( state, s ) ) { \
						caught.state = 1;
#define in_other			} else { \
						caught.state = 1;
#define in_any				} else { \
						caught.state = 1;
#define in_none_sofar			} else if ( !caught.state ) { \
						caught.state = 1;
#define on_( e )			} else if ( event == e ) { \
						caught.event = 1;
#define on_space			} else if (( event == ' ' ) || ( event == '\t' )) { \
						caught.event = 1;
#define on_separator			} else if ( is_separator( event ) ) { \
						caught.event = 1;
#define on_printable			} else if ( is_printable( event ) ) { \
						caught.event = 1;
#define on_escapable			} else if ( is_escapable( event ) ) { \
						caught.event = 1;
#define on_xdigit			} else if ( is_xdigit( event ) ) { \
						caught.event = 1;
#define on_digit			} else if ( isdigit( event ) ) { \
						caught.event = 1;
#define ons( s )			} else if ( strmatch( s, event ) ) { \
						caught.event = 1;
#define on_other			} else { \
						caught.event = 1;
#define on_any				} else { \
						caught.event = 1;
#define do_( next )				caught.transition = 1; state = (next);
#define REENTER					caught.event = 0;
#define same					state
#define end				}

#define END				end \
			} while ( !caught.event );

#endif	// MACROS_H
