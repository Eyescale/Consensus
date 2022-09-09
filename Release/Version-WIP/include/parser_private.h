#ifndef PARSER_PRIVATE_H
#define PARSER_PRIVATE_H

#define	CNCaughtEvent	1
#define	CNCaughtState	2
#define	CNCaughtTrans	4
#define	CNCaughtTest	8
#define	CNCaughtReenter	16

// #define DEBUG

//===========================================================================
//	CNParser utilities - macros
//===========================================================================
#ifdef DEBUG
#define	DBGMonitor \
	fprintf( stderr, "CNParser:l%dc%d: in \"%s\" on ", line, column, state ); \
	switch ( event ) { \
	case EOF: fprintf( stderr, "EOF" ); break; \
	case '\n': fprintf( stderr, "'\\n'" ); break; \
	case '\t': fprintf( stderr, "'\\t'" ); break; \
	default: fprintf( stderr, "'%c'", event ); \
	} \
	fprintf( stderr, "\n" );
#else
#define	DBGMonitor
#endif

#define CNParserBegin( parser ) \
			char *state	= parser->state; \
			int column	= parser->column; \
			int line	= parser->line; \
			int errnum	= 0; \
			int caught; \
			do { \
				caught = CNCaughtTest; \
				DBGMonitor; bgn_

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

#define CNParserDefault		end \
				if ( caught&CNCaughtTrans ) ; \
				else bgn_

#define CNParserEnd		end \
			} while ( !errnum && (caught&CNCaughtReenter) ); \
			parser->errnum = errnum;

//===========================================================================
//	CNParser sting utilities - macros
//===========================================================================

#define s_empty \
	!StringInformed(s)
#define s_add( str ) \
	for ( char *p=str; *p; StringAppend(s,*p++) );
#define	s_clean( a ) \
	StringReset( s, a );
#define s_take \
	StringAppend( s, event );
#if 0	// this should work, but Base/Example/1_Yak forbids...
#define s_cmp( str ) \
	strcmp( str, StringFinish(s,0) )
#else
#define s_cmp( str ) \
	strcmp( str, (s->mode==CNStringBytes?StringFinish(s,0):s->data) )
#endif


#endif	// PARSER_PRIVATE_H
