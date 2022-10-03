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
		"\t./B%% -f file.ini file.story\n"
		"\t./B%% -p file.story\n"
	);
	exit(-1);
}

int
main( int argc, char *argv[] )
{
	// interprete B% command args

	if ( argc < 2 ) usage();

	char *path[ 2 ] = { NULL, NULL };
	int printout = 0;
	if ( !strncmp( argv[1], "-p", 2 ) ) {
		if ( argc < 3 ) usage();
		path[ 0 ] = argv[ 2 ];
		printout = 1;
	}
	else if ( !strncmp( argv[1], "-f", 2 ) ) {
		if ( argc < 4 ) usage();
		path[ 0 ] = argv[ 3 ];
		path[ 1 ] = argv[ 2 ];
	}
	else {
		if ( argc < 2 ) usage();
		path[ 0 ] = argv[ 1 ];
	}

	// execute B% command

	CNStory *story = readStory( path[ 0 ] );
	if ( printout )
		story_output( stdout, story );
	else {
		CNDB *db = newCNDB();
		if (( path[ 1 ] )) cnLoad( path[ 1 ], db );
		while ( cnOperate( story, db ) )
			cnUpdate( db );
		freeCNDB( db );
	}
	freeStory( story );
}

