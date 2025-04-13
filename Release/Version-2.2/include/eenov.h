#ifndef EENOV_H
#define EENOV_H

#include "database.h"
#include "expression.h"
#include "context.h"

int eenov_output( char *, BMContext *, OutputData * );
listItem * eenov_inform( BMContext *, CNDB *, char *, BMContext * );
CNInstance * eenov_lookup( BMContext *, CNDB *, char * );
int eenov_match( BMContext *, char *, CNInstance *, CNDB * );


#endif	// EENOV_H
