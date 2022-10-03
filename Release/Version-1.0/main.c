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
		"\t./B%% -p file.story\n"
	);
	exit(-1);
}

int
main( int argc, char *argv[] )
{
	// interprete B% command args

	if ( argc < 2 ) usage();

	char *path = NULL;
	int printout = 0;
	if ( !strncmp( argv[1], "-p", 2 ) ) {
		if ( argc < 3 ) usage();
		path = argv[ 2 ];
		printout = 1;
	}
	else {
		if ( argc < 2 ) usage();
		path = argv[ 1 ];
	}

	// execute B% command

	CNDB *db = newCNDB();
	CNNarrative *narrative = readNarrative( path );
	if ( printout )
		narrative_output( stdout, narrative, 0 );
	else
		while ( cnOperate( narrative, db ) )
			cnUpdate( db );

	freeNarrative( narrative );
	freeCNDB( db );
}

