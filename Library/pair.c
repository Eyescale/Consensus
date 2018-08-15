#include <stdio.h>
#include <stdlib.h>

#include "pair.h"

Pair *
newPair( void *name, void *value )
{
	Pair *pair = (Pair *) newItem( name );
	pair->value = value;
	return pair;
}
void
freePair( Pair *pair )
	{ freeItem((listItem *) pair ); }

