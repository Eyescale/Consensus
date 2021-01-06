#include <stdio.h>
#include <stdlib.h>

#include "database.h"
#include "narrative.h"
#include "operation.h"

// #define DEBUG

void
usage( void )
{
	fprintf( stderr, "B%%: Example Usage\n"
		"\t./B%% file.story\n"
		"\t./B%% -f file.db file.story\n"
		"\t./B%% -p file.story\n"
	);
	exit(-1);
}

int
main( int argc, char *argv[] )
{
	if ( argc==1 ) usage();

	CNDB *db = NULL;
	CNStory *story = NULL;
	if ( !strncmp( argv[1], "-p", 2 ) ) {
		story = readStory( argv[ 2 ] );
		story_output( stdout, story );
	}
	else if ( !strncmp( argv[1], "-f", 2 ) ) {
		if ( argc < 4 ) usage();
		db = newCNDB();
		cnLoad( argv[ 2 ], db );
		story = readStory( argv[ 3 ] );
		while ( cnOperate( story, db ) )
			cnUpdate( db );
	}
	else {
		db = newCNDB();
		story = readStory( argv[ 1 ] );
		while ( cnOperate( story, db ) )
			cnUpdate( db );
	}
	freeStory( story );
	freeCNDB( db );
}

