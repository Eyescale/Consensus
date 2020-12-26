#ifndef DB_IO_H
#define DB_IO_H

#include "macros.h"

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
	struct { int event, state, transition; } caught; \
	do { \
		event = input( stream ); \
		do { \
                        caught.transition = 0; \
                        caught.state = 0; \
                        caught.event = 1; \
                        DBGMonitor; bgn_
#define DBInputDefault \
                        end \
                        if ( caught.transition ) ; \
                        else bgn_
#define DBInputEnd \
                        end \
                } while ( !caught.event ); \
	} while ( strcmp( state, "" ) );



#endif	// DB_IO_H
