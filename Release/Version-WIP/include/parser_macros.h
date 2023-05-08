#ifndef PARSER_MACROS_H
#define PARSER_MACROS_H

#define CB_( op, mode, data ) \
	( cb( op, mode, data ) )
#define CB_if_( op, mode, data ) \
	if CB_( op, mode, data )

//===========================================================================
//	bm_parse State Machine utilities - macros
//===========================================================================
#define	CNCaughtEvent	1
#define	CNCaughtState	2
#define	CNCaughtTrans	4
#define	CNCaughtTest	8
#define	CNCaughtReenter	16

// #define DEBUG

#ifdef DEBUG
#define	DBGMonitor( src, state, event, line, column ) \
	fprintf( stderr, "%s: l%dc%d: in \"%s\" on ", src, line, column, state ); \
	switch ( event ) { \
	case EOF: fprintf( stderr, "EOF" ); break; \
	case '\n': fprintf( stderr, "'\\n'" ); break; \
	case '\t': fprintf( stderr, "'\\t'" ); break; \
	default: fprintf( stderr, "'%c'", event ); } \
	fprintf( stderr, "\n" );
#else
#define	DBGMonitor( this, state, event, line, column )
#endif

#define BMParseBegin( this, state, event, line, column ) \
			int caught; \
			do { \
				caught = CNCaughtTest; \
				DBGMonitor( this, state, event, line, column ); bgn_

#define bgn_ 			if ( 0 ) {

#define CND_ifn( a, jmp )	} else if (!(a)) { caught&=~CNCaughtTest; goto jmp;

#define CND_if_( a, jmp )	} else if (a) { caught&=~CNCaughtTest; goto jmp;

#define CND_else_( jmp )     ;	} if ( caught&CNCaughtTest ) { goto jmp;

#define CND_endif	     ;	} caught|=CNCaughtTest; if ( caught&CNCaughtEvent ) {

#define in_( s )		} else if ( !strcmp( state, (s) ) ) { \
					caught |= CNCaughtState;
#define in_other		} else { \
					caught |= CNCaughtState;
#define in_any			} else { \
					caught |= CNCaughtState;
#define in_none_sofar		} else if (!( caught&CNCaughtState )) { \
					caught |= CNCaughtState;
#define on_( e )		} else if ( event==(e) ) { \
					caught |= CNCaughtEvent;
#define on_space		} else if ( event==' ' || event=='\t' ) { \
					caught |= CNCaughtEvent;
#define on_separator		} else if ( is_separator( event ) ) { \
					caught |= CNCaughtEvent;
#define on_printable		} else if ( is_printable( event ) ) { \
					caught |= CNCaughtEvent;
#define on_escapable		} else if ( is_escapable( event ) ) { \
					caught |= CNCaughtEvent;
#define on_xdigit		} else if ( is_xdigit( event ) ) { \
					caught |= CNCaughtEvent;
#define on_digit		} else if ( isdigit( event ) ) { \
					caught |= CNCaughtEvent;
#define ons( s )		} else if ( strmatch( s, event ) ) { \
					caught |= CNCaughtEvent;
#define on_other		} else { \
					caught |= CNCaughtEvent;
#define on_any			} else { \
					caught |= CNCaughtEvent;
#define same				state
#define do_( next )			state = (next); \
					caught |= CNCaughtTrans;
#define REENTER				caught |= CNCaughtReenter;
#define end			}

#define BMParseDefault		end \
				if ( caught&CNCaughtTrans ) ; \
				else bgn_

#define BMParseEnd		end \
			} while ( !errnum && (caught&CNCaughtReenter) );


#endif	// PARSER_MACROS_H
