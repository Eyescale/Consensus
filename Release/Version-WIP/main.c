#include <stdio.h>
#include <stdlib.h>

#include "main.h"

void
usage( void ) {
	fprintf( stderr, "B%%: Usage\n"
		"\t./B%% file.story\n"
		"\t./B%% -p file.story\n"
		"\t./B%% -f file.ini file.story\n"
		"\t./B%% -f file.ini -p\n"
		"\t./B%% -i\n"
		"\t./B%% file.bm -i\n"
		"\t./B%% -f file.ini -i\n"
		"\t./B%% -f file.ini file.bm -i\n" );
	exit(-1); }

#define bar_i( n ) \
	if ( argc==n+1 && !strncmp( argv[n], "-i", 2 ) ) \
		interactive = n; \
	else
int
main( int argc, char *argv[] ) {

	// interprete B% command args

	char *path[2] = { NULL, NULL };
	int printout = 0;
	int printini = 0;
	int interactive = 0;
	if ( argc < 2 ) usage();

	if ( !strncmp( argv[1], "-p", 2 ) ) {
		if ( argc < 3 ) usage();
		path[1] = argv[2];
		printout = 1; }
	else if ( !strncmp( argv[1], "-f", 2 ) ) {
		if ( argc < 4 ) usage();
		path[0] = argv[2];
		bar_i( 3 ) {
			path[1] = argv[3];
			printini = !strcmp( path[1], "-p" );
			bar_i( 4 ) ; } }
	else bar_i( 1 ) {
		path[1] = argv[1];
		bar_i( 2 ) ; }

	// execute B% command

	if ( printini )
		cnPrintout( stdout, path[0], 1 );
	else if ( printout ) {
		CNStory *story = readStory( path[1], 0 );
		cnStoryOutput( stdout, story );
		freeStory( story ); }
	else if ( interactive ) {
		CNStory *story = readStory( path[1], 1 );
		CNProgram *threads = newProgram( story, path[0] );
		CNCell *this = CNProgramStem( threads );
		do cnSync(threads); while ( cnOperate( &this, threads ) );
		freeProgram( threads );
		freeStory( story ); }
	else {
		CNStory *story = readStory( path[1], 0 );
		CNProgram *threads = newProgram( story, path[0] );
		do cnSync(threads); while ( cnOperate( NULL, threads ) );
		freeProgram( threads );
		freeStory( story ); }

	cnExit( 1 ); }

