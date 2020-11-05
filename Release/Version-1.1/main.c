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
	CNStory *story = readStory( argv[ 1 ] );
#ifdef DEBUG
	story_output( stderr, story, 0 );
	exit( 0 );
#endif
	while ( cnOperate( story, db ) )
		cnUpdate( db );

	freeStory( story );
	freeCNDB( db );
}

