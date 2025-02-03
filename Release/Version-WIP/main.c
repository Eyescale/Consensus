#include <stdio.h>
#include <stdlib.h>

#include "main.h"

void
usage( ProgramData *data ) {
	if (( data )) freeListItem( &data->flags );
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
		data.interactive = n; \
	else
int
main( int argc, char *argv[] ) {

	if ( argc < 2 ) usage( NULL );

	ProgramData data;
	memset( &data, 0, sizeof(data) );

	int i;
	for ( i=1; i<argc && !strncmp(argv[i],"-D",2); i++ )
		addItem( &data.flags, argv[i]+2 );
	i--; argc-=i; argv+=i;

	// interprete B% command args

	char *inipath, *storypath;
	int printout = 0;
	int printini = 0;

	if ( !strncmp( argv[1], "-p", 2 ) ) {
		if ( argc < 3 ) usage( &data );
		storypath = argv[2];
		printout = 1; }
	else if ( !strncmp( argv[1], "-f", 2 ) ) {
		if ( argc < 4 ) usage( &data );
		inipath = argv[2];
		bar_i( 3 ) {
			if ( !strcmp( argv[3], "-p" ) )
				printini = 1;
			else storypath = argv[3];
			bar_i( 4 ) ; } }
	else bar_i( 1 ) {
		storypath = argv[1];
		bar_i( 2 ) ; }

	// execute B% command

	cnInit();
	if ( printini )
		cnPrintOut( stdout, inipath, data.flags );
	else if ( printout ) {
		CNStory *story = readStory( storypath, data.flags );
		cnStoryOutput( stdout, story );
		freeStory( story ); }
	else {
		CNStory *story = readStory( storypath, data.flags );
		CNProgram *threads = newProgram( inipath, &story, &data );
		do cnSync(threads); while ( cnOperate( threads, &data ) );
		freeProgram( threads, &data );
		freeStory( story ); }
	freeListItem( &data.flags );
	cnExit( 1 ); }

