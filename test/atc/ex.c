#include <stdio.h>
#include <stdlib.h>

int
main( int argc, char *argv[] )
{
	char *cmd;
	asprintf( &cmd, "./test/atc/at %s | ./consensus -m", argv[ 1 ] );
	system( cmd );
	return 0;
}
