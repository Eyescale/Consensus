#ifndef DEBUG_H
#define DEBUG_H

void	WRN_group_not_found( SchemaThread *s );
void	WRN_RX_multiple_results( void );

#ifdef DEBUG //----------------------- DEBUG -------------------------

void	DBG_RX_expect( SchemaThread *s );
void	DBG_RX_position( SchemaThread *s );
void	DBG_RX_fork( SchemaThread *s, SchemaThread *clone );

#define	DBG_RX_FRAME_START \
	fprintf( stderr, "\tRX FRAME: start\n" );
#define	DBG_RX_PREPROCESS_DONE \
	fprintf( stderr, "\tRX PREPROCESS: done\n" );
#define	DBG_RX_CR \
	fprintf( stderr, "\n" );
#define	DBG_RX_REBASE \
	fprintf( stderr, "\tRX REBASE\n" );
#define	DBG_RX_FAILED \
	fprintf( stderr, " -> failed\n" );
#define	DBG_RX_OUT(s) \
	fprintf( stderr, "\t\tRX s:%0x - out\n", (int) s );
#define	DBG_RX_FLUSH(s) \
	fprintf( stderr, "\t\tRX s:%0x - flush\n", (int) s );
#define	DBG_RX_LINE \
	fprintf( stderr, "~~~~~~\n" );
#define	DBG_RX_NFORK(t) \
	fprintf( stderr, "\t\tRX s:%0x", (int) t )

#define BINGO( num ) \
	fprintf( stderr, "BINGO %d\n", num );

#else	//------------------------- NOT DEBUG ----------------------------

#define	DBG_RX_expect(a)
#define	DBG_RX_position(a)
#define	DBG_RX_fork(a,b)

#define	DBG_RX_FRAME_START
#define	DBG_RX_PREPROCESS_DONE
#define	DBG_RX_CR
#define	DBG_RX_REBASE
#define	DBG_RX_FAILED
#define	DBG_RX_OUT(a)
#define	DBG_RX_FLUSH(a)
#define	DBG_RX_NFORK(a)
#define DBG_RX_RULE_FOUND(r)

#define BINGO( a )

#endif	//---------------------------- / ------------------------------
#endif	// DEBUG_H
