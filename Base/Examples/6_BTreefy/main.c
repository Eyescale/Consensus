#include <stdio.h>
#include <stdlib.h>

#include "btree.h"

//===========================================================================
//	main()	- TEST
//===========================================================================
char *sequence =
//	"(hello,world)"
//	"(hello):(titi,toto)"
//	"alpha:beta((%?,*toto:~(billy,.boy)),%(tata:%ernest,%)))"
//	":alpha:beta:((%?,*toto:~(billy,.boy)),%(tata:%ernest,%)))"
//	"{alpha,beta:(hello,(world):(titi,toto)),gamma,delta}"
	"{alpha,beta:({hello,hi},(world)),gamma,delta}"
//	"((%?,*toto:~(billy,.boy)),%(tata:%ernest,%)))"
//	"alpha:beta"
//	"%(**?)"
	;

int
main( int argc, char *argv[] )
{
	BTreeNode *btree = btreefy( sequence );
	printf( "Sequence\n\t%s\nTree\n", sequence );
	output_btree( btree, 1 );
	freeBTree( btree );
}

