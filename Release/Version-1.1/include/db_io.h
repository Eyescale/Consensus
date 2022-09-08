#ifndef DB_IO_H
#define DB_IO_H

#include "macros_private.h"

//===========================================================================
//      DBInputBegin / DBInputEnd - macros
//===========================================================================
#ifdef DEBUG
#define DBGMonitor \
	fprintf( stderr, "DBInput: in \"%s\" on '%c'\n", state, event );
#else
#define DBGMonitor
#endif

#define DBInputBegin( stream, input ) \
	char *state = "base"; \
	int event; \
	int caught; \
	do { \
		event = input( stream ); \
		do { \
			caught = CNCaughtEvent; \
			DBGMonitor; bgn_
#define DBInputDefault \
			end \
			if ( caught & CNCaughtTrans ) ; \
			else bgn_
#define DBInputEnd \
			end \
		} while ( !(caught & CNCaughtEvent) ); \
	} while ( strcmp( state, "" ) );



#endif	// DB_IO_H

