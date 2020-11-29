#ifndef EXPRESSION_PRIVATE_H
#define EXPRESSION_PRIVATE_H

#include "macros.h"

//===========================================================================
//      Expression BMInputBegin / BMInputEnd - macros
//===========================================================================
#ifdef DEBUG
#define DBGMonitor \
        fprintf( stderr, "BMInput: in \"%s\" on '%c'\n", state, event );
#else
#define DBGMonitor
#endif

#define BMInputBegin( stream, input ) \
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
#define BMInputDefault \
                        end \
                        if ( caught.transition ) ; \
                        else bgn_
#define BMInputEnd \
                        end \
                } while ( !caught.event ); \
	} while ( strcmp( state, "" ) );



#endif	// EXPRESSION_PRIVATE_H
