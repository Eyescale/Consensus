#ifndef PARSER_DEBUG_H
#define PARSER_DEBUG_H

#ifdef DEBUG //----------------------- DEBUG -------------------------

void	DBG_event( int event );
void	DBG_frameno( listItem *frames );
void	DBG_expect( SchemaThread *s );
void	DBG_position( SchemaThread *s );
void	DBG_fork( SchemaThread *s, SchemaThread *clone );
void	DBG_rule_found( char * );

#define	DBG_CR \
	fprintf( stderr, "\n" );
#define	DBG_REBASE \
	fprintf( stderr, "REBASE\n" );
#define	DBG_FAILED \
	fprintf( stderr, " -> failed\n" );
#define	DBG_OUT(s) \
	fprintf( stderr, "\ts:%0x - out\n", (int) s );
#define	DBG_FLUSH(s) \
	fprintf( stderr, "\ts:%0x - flush\n", (int) s );
#define	DBG_LINE \
	fprintf( stderr, "~~~~~~\n" );
#define	DBG_NFORK(t) \
	fprintf( stderr, "\ts:%0x", (int) t )

#else	//------------------------- NOT DEBUG ----------------------------

#define	DBG_event(a)
#define	DBG_frameno(a)
#define	DBG_expect(a)
#define	DBG_position(a)
#define	DBG_fork(a,b)
#define	DBG_rule_found(a)

#define	DBG_CR
#define	DBG_REBASE
#define	DBG_FAILED
#define	DBG_OUT(a)
#define	DBG_FLUSH(a)
#define	DBG_NFORK(a)
#define DBG_RULE_FOUND(r)

#endif	//---------------------------- / ------------------------------
#endif	// PARSER_DEBUG_H
