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

static int
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
	map cosystem changes into internal events
*/
static int
ParserSync( System *this )
{
	CNSyncBegin( ParserData )
	CNOnSystemChange( InputID ) bgn_
		CNOnFieldChange( InputData, event )
			CNDoi( input.event, CNFieldValue( InputData, event ) );
		end
	CNSyncEnd
}
