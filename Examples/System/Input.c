#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include "system.h"
#include "Input.h"

/*---------------------------------------------------------------------------
	InputInit
---------------------------------------------------------------------------*/
static Narrative InputFrame;
static Narrative InputSync;

int
InputInit( System *this )
{
	this->data = calloc( 1, sizeof(InputData) );
	this->init = InputInit;
	this->frame = InputFrame;
	this->sync = InputSync;
	return 1;
}

/*---------------------------------------------------------------------------
	InputFrame
---------------------------------------------------------------------------*/
#include "id.h"
#include "Parser.h"

static int input( void );

static int
InputFrame( System *this )
{
	CNFrameBegin( InputData )
	if ( init ) {
		init = 0;
		CNSubscriptionBegin( ParserID )
			CNRegister( ParserData, call.input )
		CNSubscriptionEnd
		}
	if ( CNOn( parser.call ) ) {
		CNDoi( event, input() );
		}
	CNFrameEnd
}

static int
input( void )
{
	int event = 0;
	read( STDIN_FILENO, &event, 1 );
	return event;
}

/*---------------------------------------------------------------------------
	InputSync
---------------------------------------------------------------------------*/
/*
   Notes
   . Here we log the local address, consistently with CNOn( parser.call )
     Alternatively we could log the interest itself and invoke CNOn( parser->call.input )
     The benefit is lesser memory consumption (no local mapping of cosystem data)
     The risk is a race condition on parser data during the next frame
*/
static int
InputSync( System *this )
{
	CNSyncBegin( InputData )
	CNOnSystemChange( ParserID ) bgn_
		CNOnFieldChange( ParserData, call.input )
			CNSyncData( parser.call ) = 1;
			CNSyncLog( parser.call );
		end
	CNSyncEnd
	return 1;
}

