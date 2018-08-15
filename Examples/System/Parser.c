#include <stdio.h>
#include <stdlib.h>

#include "system.h"
#include "Parser.h"

/*---------------------------------------------------------------------------
	ParserInit
---------------------------------------------------------------------------*/
static Narrative ParserFrame;
static Narrative ParserSync;

int
ParserInit( System *this )
{
	this->data = calloc( 1, sizeof(ParserData) );
	this->init = ParserInit;
	this->frame = ParserFrame;
	this->sync = ParserSync;
	return 1;
}

/*---------------------------------------------------------------------------
	ParserFrame
---------------------------------------------------------------------------*/
#include "id.h"
#include "Input.h"

int
ParserFrame( System *this )
{
	CNFrameBegin( ParserData )
	if ( init ) {
		init = 0;
		CNSubscriptionBegin( InputID )
			CNRegister( InputData, event )
		CNSubscriptionEnd
		CNDoi( call.input, 1 );
		}
	if ( CNIni( state, 0 ) ) {
		if ( CNOni( input.event, '\n' ) ) {
			fprintf( stderr, "hello, world\n" );
			CNDoi( call.input, 1 );
			}
		else if ( CNOni( input.event, '/' ) ) {
			CNDoi( state, 1 );
			CNDoi( call.input, 1 );
			}
		else if ( CNOn( input.event ) ) {
			CNDoi( call.input, 1 );
			}
		}
	else {
		if ( CNOn( input.event ) ) {
			if ( !CNOni( input.event, '\n' ) ) {
				CNDoi( state, 0 );
				CNDoi( call.input, 1 );
				}
			}
		}
	CNFrameEnd
}

/*---------------------------------------------------------------------------
	ParserSync
---------------------------------------------------------------------------*/
/*
   Notes
   . Here we log the local address, consistently with CNOn( input.event )
     Alternatively we could log the interest itself and invoke CNOn( input->event )
     The benefit is lesser memory consumption (no local mapping of cosystem data)
     The risk is a race condition on input data during the next frame
*/
static int
ParserSync( System *this )
{
	CNSyncBegin( ParserData )
	CNOnSystemChange( InputID ) bgn_
		CNOnFieldChange( InputData, event )
			CNSyncData( input.event ) = CNChangeValue( InputData, event );
			CNSyncLog( input.event );
		end
	CNSyncEnd
}
