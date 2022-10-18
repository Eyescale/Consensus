#ifndef PARSER_PRIVATE_H
#define PARSER_PRIVATE_H

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
#define	DBGMonitor \
	fprintf( stderr, "bm_parse:l%dc%d: in \"%s\" on ", line, column, state ); \
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

#define BMParseBegin( parser ) \
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

#define BMParseDefault		end \
				if ( caught&CNCaughtTrans ) ; \
				else bgn_

#define BMParseEnd		end \
			} while ( !errnum && (caught&CNCaughtReenter) ); \
			parser->errnum = errnum;


#define WarnEntireCNDBCoupling \
	if ( *type&DO && is_f(LEVEL) && !is_f(SUB_EXPR|NEGATED|FILTERED) ) \
		bm_parse_report( data, ErrEntireCNDBCoupling, mode );

//===========================================================================
//	bm_parse string utilities - macros
//===========================================================================

#define s_empty \
	!StringInformed(s)
#define	s_at( event ) \
	( StringAt(s)==event )
#define s_add( str ) \
	for ( char *_c=str; *_c; StringAppend(s,*_c++) );
#define	s_reset( a ) \
	StringReset( s, a );
#define s_take \
	StringAppend( s, event );
#define s_cmp( str ) \
	strcmp( str, StringFinish(s,0) )


#endif	// PARSER_PRIVATE_H
