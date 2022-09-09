#ifndef SMGEN_H
#define SMGEN_H

#include "list.h"

int	smVerify( char *expr[], int nums );
listItem * smBuild( char *expr[], int nums );
void	smOutput( listItem *sm, int tab );
void	smFree( listItem **sm );

//===========================================================================
//      SMBegin / SMEnd - macros
//===========================================================================
#include "macros_private.h"

#ifdef DEBUG
#define DBGMonitor \
        fprintf( stderr, "smgen: in \"%s\" on '%c'\n", state, event );
#else
#define DBGMonitor
#endif

#define SMBegin( expr, nums ) \
	char *state; \
	int line, column, event; \
	for ( int line=0; line<nums; line++ ) { \
		char *str = expr[ line ]; \
		state = "base"; column = 0; \
		struct { int event, state, transition; } caught; \
		do { \
			event = str[ column++ ]; \
			do { \
				caught.transition = 0; \
				caught.state = 0; \
				caught.event = 1; \
				DBGMonitor; bgn_
#define SMDefault \
				end \
				if ( caught.transition ) ; \
				else bgn_
#define SMEnd \
	                        end \
			} while ( !caught.event ); \
		} while ( strcmp( state, "" ) );\
	}



#endif	// SMGEN_H
