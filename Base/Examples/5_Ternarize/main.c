#include <stdio.h>
#include <stdlib.h>

#include "list.h"
#include "pair.h"
#include "flags.h"
#include "string_util.h"
#include "ternarize.h"

static int
ternary_pass( char *guard, void *user_data )
{
	printf( "\tternary_pass: %s ", guard );
	int success =
		!strncmp( guard, "(pass", 5 ) ||
		!strncmp( guard, "(?:pass", 7 );
	printf( success ? ".\n" : "-> ~\n" );
	return success;
}

//===========================================================================
//	main
//===========================================================================
// char *expression = "( hello, world )";
// char *expression = "( hi:bambina ? hello : world )";
// char *expression = "( hi:bambina ?: carai )";
// char *expression = "chiquita:( hi:bambina ? caramba :):bambam, tortilla";
// char *expression = "( hi ? hello : You ? world : caramba )";
// char *expression = "( hi ? hello ? You :)";
char *expression = "( hello, world ):%( hi ? hello ? You : caramba ):( alpha, ( beta ? (gamma?epsilon:kappa) : delta ) )";
// char *expression = "( hi ? hello ? You :: caramba )";
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
#if 0
	printf( "\ndeternarized:\n" );
		char *p = deternarize( expression, ternary_pass, NULL );
	printf( "\nresult:\n" );
		if (( p )) { printf( "\t%s\n", p ); free( p ); }
		else printf( "\tas is" );
#endif
}

