#include <stdio.h>
#include <stdlib.h>

#include "database.h"
#include "narrative.h"
#include "operation.h"

// #define DEBUG

int
main( int argc, char *argv[] )
{
	if ( argc < 2 ) {
		fprintf( stderr, "Example usage: ./B%% Test/CN.suite\n" );
		exit( - 1 );
	}
	CNDB *db = newCNDB();
	CNNarrative *narrative = readNarrative( argv[ 1 ] );
#ifdef DEBUG
	narrative_output( stderr, narrative, 0 );
	exit( 0 );
#endif
	while ( cnOperate( narrative, db ) )
		cnUpdate( db );

	freeNarrative( narrative );
	freeCNDB( db );
}

