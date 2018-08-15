#include <stdio.h>
#include <stdlib.h>

#include "system.h"
#include "id.h"

/*---------------------------------------------------------------------------
	main
---------------------------------------------------------------------------*/
int
main( int argc, char **argv )
{
	CNSystemBegin( operator );
		CNSubSystem( InputID, InputInit );
		CNSubSystem( ParserID, ParserInit );
	CNSystemEnd();

	while( CNSystemFrame( operator ) );

	CNSystemFree( operator );
}

