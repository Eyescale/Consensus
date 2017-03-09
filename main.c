#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "database.h"
#include "registry.h"
#include "kernel.h"

#include "api.h"
#include "command.h"
#include "variables.h"

// #define DEBUG

/*---------------------------------------------------------------------------
	main
---------------------------------------------------------------------------*/
int
main( int argc, char ** argv )
{
	_context context;
	bzero( &context, sizeof(_context) );
	CN.context = &context;

	CN.this = newEntity( NULL, NULL, NULL );
	CN.nil = newEntity( NULL, NULL, NULL );
	CN.nil->sub[0] = CN.nil;
	CN.nil->sub[1] = CN.nil;
	CN.nil->sub[2] = CN.nil;
	CN.nil->state = 1;	// always on

	StackVA stack;
	bzero( &stack, sizeof(StackVA) );
	set_this_variable( &stack.variables, CN.this );

	registerByName( &CN.VB, "name", NULL );
	registerByName( &CN.VB, "hcn", NULL );
	registerByName( &CN.VB, "url", NULL );
	registerByName( &CN.VB, "narratives", NULL );

	context.control.mode = ExecutionMode;
	context.control.stack = newItem( &stack );
	context.control.prompt = 1;
	context.hcn.state = "";

	return read_command( base, 0, &same, &context );
}

