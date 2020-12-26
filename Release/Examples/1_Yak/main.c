#include <stdio.h>
#include <stdlib.h>

#include "database.h"
#include "narrative.h"
#include "operation.h"

// #define DEBUG

void usage( void )
{ fprintf( stderr, "Usage: ./B%% [-p] file.story [file.db]\n" ); }

int
main( int argc, char *argv[] )
{
	if ( argc==1 || ( argc==2 && argv[1][0]=='-' ))
		{ usage(); exit( -1 ); }

	CNStory *story = NULL;
	if ( !strncmp( argv[1], "-p", 2 ) ) {
		story = readStory( argv[ 2 ] );
		story_output( stdout, story );
	}
	else {
		CNDB *db = newCNDB();
		if ( argc==3 ) cnLoad( argv[ 2 ], db );
		story = readStory( argv[ 1 ] );
		while ( cnOperate( story, db ) )
			cnUpdate( db );
		freeCNDB( db );
	}
	freeStory( story );
}

