#ifndef WILDCARD_H
#define WILDCARD_H

#include "btree.h"

/* Optimizes db_verify - cf expression.c
   Evaluates the following conditions for a wildcard candidate:
	. Firstly, if it's a mark, that it is not starred
	. Secondly that it is neither base nor filtered - that is, if it
	  belongs to a filtered expression, then it should be the last term
	  of that expression, and in that case all the other filter terms
	  must be wildcards.
	. Thirdly, that it is the second term of a couple.
	. Fourthly, that the first term of that couple is also a wildcard.
	. Lastly, that if either term of that couple is marked, then that
	  the sub-expression to which the couple pertains does not contain
	  other couples or non-wildcards terms.
   Failing any of these conditions (with the exception of the first) will
   cause wildcard_opt() to return 1 - thereby allowing db_verify() to bypass
   any further inquiry on the wildcard, which can result in substantial
   performance optimization depending on the CNDB size.
   Otherwise, that is: either failing the first condition or meeting all
   of the others, wildcard_opt() will return 0, requiring db_verify() to
   perform a full evaluation on the wildcard.
*/
int wildcard_opt( char *p, BTreeNode *root );


#endif	// WILDCARD_H
