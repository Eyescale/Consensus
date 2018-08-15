#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "regex.h"
#include "regex_util.h"

#define DEBUG
#include "regex_debug.h"

/*---------------------------------------------------------------------------
	Warnings
---------------------------------------------------------------------------*/
void
WRN_group_not_found( SchemaThread *s )
{
	fprintf( stderr, "RX Warning: group '%%(' not found\n" );
}
void
WRN_RX_multiple_results( void )
{
	fprintf( stderr, "RX Warning: multiple results\n" );
}
void
DBG_RX_expect( SchemaThread *s )
{
	RegexGroup *group = s->g->group;
	char str[ 3 ];
	switch ( s->position[ 0 ] ) {
		case '(':
			fprintf( stderr, "\t\tRX s:%0x: pending on %%(", (int)s );
			return;
		case '\\':
			str[ 0 ] = '\\';
			str[ 1 ] = s->position[ 1 ];
			str[ 2 ] = (char) 0;
			break;
		default:
			str[ 0 ] = s->position[ 0 ];
			str[ 1 ] = (char) 0;
	}
	fprintf( stderr, "\t\tRX s:%0x (: expecting '%s'", (int)s, str );
}
void
DBG_RX_position( SchemaThread *s )
{
	char *position = s->position;
	switch ( position[ 0 ] ) {
		case '|':
		case ')':
		case '/':
			fprintf( stderr, " -> checked & resolved\n" );
			break;
		case '(': ;
			fprintf( stderr, " -> checked & new position %%(\n" );
			break;
		case '\\':
			fprintf( stderr, " -> checked & new position '\\%c'\n", position[ 1 ] );
			break;
		default:
			fprintf( stderr, " -> checked & new position '%c'\n", position[ 0 ] );
	}
}
void
DBG_RX_fork( SchemaThread *s, SchemaThread *clone )
{
	fprintf( stderr, "\t\tRX s:%0x forked s:%0x", (int)s, (int)clone );
}
