#ifndef EENO_H
#define EENO_H

#include "database.h"
#include "context.h"

int eeno_output( BMContext *, int type, char * );
CNInstance * eeno_inform( BMContext *, CNDB *, char *, BMContext * );
CNInstance * eeno_lookup( BMContext *, CNDB *, char * );
int eeno_match( BMContext *, char *, CNDB *, CNInstance * );

int db_x_match( CNDB *, CNInstance *, CNDB *, CNInstance * );



#endif	// EENO_H
