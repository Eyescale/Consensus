#include <stdio.h>
#include <stdlib.h>

#include "list.h"
#include "pair.h"
#include "flags.h"
#include "string_util.h"
#include "ternarize.h"

static int
ternary_check( char *guard, void *user_data )
{
	printf( "\tternary_check: %s ", guard );
	int success = !strncmp( guard, "pass", 4 );
	printf( success ? ".\n" : "-> ~\n" );
	return success;
}

//===========================================================================
//	main
//===========================================================================
// char *expression = "( hello, world )";
// char *expression = "%( bambina ? hello : world )";
// char *expression = "( bambina ? world :)";
// char *expression = "( hi:bambina ? hello : world )";
// char *expression = "( hi:bambina ?: carai )";
// char *expression = "chiquita:( hi:bambina ? caramba :):bambam, tortilla";
// char *expression = "( hi ? hello : You ? world : caramba )";
// char *expression = "( hi ? hello ? You :)";
// char *expression = "( hello, world ):%( hi ? hello ? You : caramba ):( alpha, ( beta ? %(gamma?epsilon:kappa) : delta ) )";
// char *expression = "( hello, world ):%( hi ? hello ? You : caramba ):( alpha, ( pass ? %(gamma?epsilon:kappa) : delta ) )";
char *expression = "( hi ? hello ? You :: caramba )";
// char *expression = "( hi ? hello ?: world : caramba )";

int
main( int argc, char *argv[] )
{
	printf( "expression:\n\t" );
        for ( char *p=expression; *p; p++ )
		switch ( *p ) {
		case '\n':
			printf( "\\n" );
			break;
		default:
                	putchar( *p );
		}
        printf( "\nternarized:\n" );
		listItem *sequence = ternarize( expression );
		output_ternarized( sequence );
		free_ternarized( sequence );
	printf( "deternarized:\n" );
		char *p = deternarize( expression, ternary_check, NULL );
		if ( p ) {
			printf( "result:\n" );
			printf( "\t%s\n", p );
			free( p );
		}
		else printf( "\tas is\n" );
}

