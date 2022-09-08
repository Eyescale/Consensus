#ifndef PARSER_PRIVATE_H
#define PARSER_PRIVATE_H

#define s_empty \
	!StringInformed(s)
#define s_take \
	StringAppend( s, event );
#define s_cmp( str ) \
	strcmp( str, StringFinish(s,0) )
#define s_add( str ) \
	for ( char *p=str; *p; StringAppend(s,*p++) );
#define	s_clean( a ) \
	StringReset( s, a );

// #define DEBUG

//===========================================================================
//	CNParser utilities - macros
//===========================================================================
// CND_reset is used in conjunction with CND_if_ and CND_ifn statements,
// to be applied e.g. as follow within a block:
// 		in_( ) CND_reset bgn_
// CND_if_( a, A )
//			on_( ) ...
// A:CND_else_( B )
//			on_( ) ...
// B:CND_endif
//			on_( ) ...
//			end
// CND_reset
//		in_( ) bgn_
//			etc.
#define CND_reset \
	cond=1, passed=1;

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

// Note CND_ifn does not allow CND_else_ (being a straight on_goto )
// only CND_if_ does

#define CNParserBegin( parser ) \
	char *state	= parser->state; \
	int column	= parser->column, \
	    line	= parser->line, \
	    errnum	= 0; \
	int caught, cond, passed; \
	do { \
		caught = CNCaughtEvent; \
		DBGMonitor; bgn_
#define CND_ifn( a, jmp ) \
		} else if (!(a)) { passed=0; goto jmp;
#define CND_if_( a, jmp ) \
		} else if (a) { cond=0; goto jmp;
#define CND_else_( jmp ) ; \
		} else passed=0; if ( cond==1 ) { goto jmp;
#define CND_endif	; \
		} else passed=0; if ( passed ) {
#define CNParserDefault \
		end \
		if ( caught & CNCaughtTrans ) ; \
		else bgn_
#define CNParserEnd \
		end \
	} while ( !errnum && !(caught&CNCaughtEvent) ); \
	parser->errnum = errnum;

#define case_(a)	switch (a) { case 0:
#define _(a)		break; case a:
#define _default	break; default:
#define	_end_case	break; }

#endif	// PARSER_PRIVATE_H
