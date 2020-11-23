#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "parser.h"
#include "parser_util.h"

/*---------------------------------------------------------------------------
	Debug
---------------------------------------------------------------------------*/
void
DBG_rule_found( char *identifier )
{
	fprintf( stderr, "Debug: rule '%s' already there\n", identifier );
}
void
DBG_event( int event )
{
	switch ( event ) {
		case EOF: fprintf( stderr, " EOF\t" ); break;
		case '\t': fprintf( stderr, " \\t\t" ); break;
		case '\n': fprintf( stderr, " \\n\t" ); break;
		case ' ': fprintf( stderr, " ' '\t" ); break;
		default: fprintf( stderr, " %c\t", event );
	}
}
void
DBG_frameno( listItem *frames )
{
	int n = 0;
	for ( listItem *i=frames; i!=NULL; i=i->next )
		n++;
	fprintf( stderr, "#framelog: %d\n", n );
}
void
DBG_expect( SchemaThread *s )
{
	Rule *rule = s->r->rule;
	int inbuilt = rule->identifier[ 0 ];
	if ( inbuilt && is_separator( inbuilt ) ) {
		fprintf( stderr, "\ts:%0x %s: active", (int)s, rule->identifier );
		return;
	}
	char str[ 3 ], *identifier;
	switch ( s->position[ 0 ] ) {
		case '%':
			if ( is_separator( s->position[ 1 ] ) ) {
				fprintf( stderr, "\ts:%0x %s: pending on %%%c",
					(int)s, rule->identifier, s->position[ 1 ] );
				return;
			}
			strscanid( s->position, &identifier );
			fprintf( stderr, "\ts:%0x %s: pending on %%%s", (int)s, rule->identifier, identifier );
			free( identifier );
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
	fprintf( stderr, "\ts:%0x %s: expecting '%s'", (int)s, rule->identifier, str );
}
void
DBG_position( SchemaThread *s )
{
	char *position = s->position, *identifier;
	switch ( position[ 0 ] ) {
		case '\0':
			fprintf( stderr, " -> checked & resolved\n" );
			break;
		case '%': ;
			strscanid( position, &identifier );
			if (( identifier )) {
				fprintf( stderr, " -> checked & new position %%%s\n", identifier );
				free( identifier );
			}
			else fprintf( stderr, " -> checked & new position %%%c\n", position[ 1 ] );
			break;
		case '\\':
			fprintf( stderr, " -> checked & new position '\\%c'\n", position[ 1 ] );
			break;
		default:
			fprintf( stderr, " -> checked & new position '%c'\n", position[ 0 ] );
	}
}
void
DBG_fork( SchemaThread *s, SchemaThread *clone )
{
	fprintf( stderr, "\ts:%0x forked s:%0x", (int)s, (int)clone );
}
