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
	int column	= parser->column, \
	    line	= parser->line, \
	    errnum	= 0; \
	int caught; \
	do { \
		caught = CNCaughtTest; \
		DBGMonitor; bgn_
#define CND_ifn( a, jmp ) \
		} else if (!(a)) { caught&=~CNCaughtTest; goto jmp;
#define CND_if_( a, jmp ) \
		} else if (a) { caught&=~CNCaughtTest; goto jmp;
#define CND_else_( jmp ) ; \
		} if ( caught&CNCaughtTest ) { goto jmp;
#define CND_endif	; \
		} caught|=CNCaughtTest; if ( caught&CNCaughtEvent ) {
#define CNParserDefault \
		end \
		if ( caught & CNCaughtTrans ) ; \
		else bgn_
#define CNParserEnd \
		end \
	} while ( !errnum && (caught&CNCaughtAgain) ); \
	parser->errnum = errnum;

#endif	// PARSER_PRIVATE_H
