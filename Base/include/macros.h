#ifndef MACROS_H
#define MACROS_H

#define BEGIN		int reenter; \
			do { \
				reenter = 0; bgn_
#define bgn_				if ( 0 ) {
#define in_( s )			} else if ( !strcmp( state, s ) ) {
#define in_other			} else {
#define in_any				} else {
#define on_( e )			} else if ( event == e ) {
#define on_space			} else if (( event == ' ' ) || ( event == '\t' )) {
#define on_separator			} else if ( is_separator( event ) ) {
#define on_digit			} else if ( isdigit( event ) ) {
#define on_other			} else {
#define on_any				} else {
#define same					state
#define do_( next )				if ( next != state ) state = next;
#define REENTER					reenter = 1;
#define end				}
#define END				end \
			} while ( reenter );

#endif	// MACROS_H
