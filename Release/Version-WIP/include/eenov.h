#ifndef EENOV_H
#define EENOV_H

#include "database.h"
#include "context.h"

int eenov_output( BMContext *, int type, char * );
CNInstance * eenov_inform( BMContext *, CNDB *, char *, BMContext * );
CNInstance * eenov_lookup( BMContext *, CNDB *, char * );
int eenov_match( BMContext *, char *, CNDB *, CNInstance * );


#endif	// EENOV_H
