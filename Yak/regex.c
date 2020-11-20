#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "regex.h"
#include "regex_util.h"
// #define DEBUG
#include "regex_debug.h"

/*===========================================================================

	Regex public interface -- implementation

===========================================================================*/
static void
WRN_RX_multiple_results( void )
	{ fprintf( stderr, "RX Warning: multiple results\n" ); }

enum {
	PREPROCESS = 0,
	ACTIVATE,
	DEACTIVATE,
	RELEASE,
	NO_CHANGE
};
#define DECLARE( change ) \
	listItem *change[ NO_CHANGE ]; \
	for ( int i=0; i<NO_CHANGE; i++ ) \
		change[ i ] = NULL;

/*---------------------------------------------------------------------------
	newRegex
---------------------------------------------------------------------------*/
static listItem *g_build( char *fmt );
static void r_probe( Regex *r, int *passthrough );
static int regerr( char *msg )
	{ fprintf( stderr, "Error: Regex: %s - unsupported\n", msg ); return -1; }

Regex *
newRegex( char *fmt, int *passthrough )
{
	Regex *regex = calloc( 1, sizeof(Regex) );
	if ( regex == NULL ) return NULL;

	regex->groups = g_build( fmt );
	if ( regex->groups == NULL ) return NULL;
	r_probe( regex, passthrough );

	addItem( &regex->frames, NULL );
	RegexRebase( regex, R_BASE );
	return regex;
}

#define newSchema( lptr, p )	addItem( lptr, p )
static listItem *
g_build( char *fmt )
{
	struct { int min; } range;
	listItem *stack = NULL;
	listItem *schema = NULL; // only used as a flag
	listItem *groups = NULL;
	RegexGroup *group = (RegexGroup *) newPair( fmt++, NULL );
	addItem( &groups, group );

	char *p = NULL;
	CNStringStreamBegin( fmt, p, "base", status )
	on_( '\0' )
		status = regerr( "regex unclosed" );
	in_( "base" ) bgn_
		on_( '/' )	do_( "" )	status = (!(stack)) ? +1 : regerr( "unclosed '('" );
		on_( '?' )	do_( "" )	status = regerr( "leading '?' (unescaped)" );
		on_( '*' )	do_( "" )	status = regerr( "leading '*' (unescaped)" );
		on_( '+' )	do_( "" )	status = regerr( "leading '+' (unescaped)" );
		on_( '{' )	do_( "{" )	status = regerr( "leading '{' (unescaped)" );
		on_( '(' )	do_( "(" )
					if (!(schema)) schema = newSchema( &group->schemata, p );
					addItem( &stack, group );
					group = (RegexGroup *) newPair( p, NULL );
					addItem( &groups, group );
					schema = NULL;
		on_( ')' )	do_( "_" )
					if (!(stack)) status = regerr( "unwarranted ')' (unescaped)" );
					else group = popListItem( &stack );
		on_( '|' )	do_( "|" )	schema = NULL;
		on_( '[' )	do_( "[" )	if (!(schema)) schema = newSchema( &group->schemata, p );
		on_( '\\' )	do_( "\\" )	if (!(schema)) schema = newSchema( &group->schemata, p );
		on_other	do_( "_" )	if (!(schema)) schema = newSchema( &group->schemata, p );
		end
	in_( "_" ) bgn_
		on_( '?' )	do_( "base" )
		on_( '*' )	do_( "base" )
		on_( '+' )	do_( "base" )
		on_( '{' )	do_( "{" )
		on_other	do_( "base" )	REENTER
		end
	in_( "{" ) bgn_
		on_( '}' )	do_( "" )	status = regerr( "empty '{}'" );
		on_( ',' )	do_( "{," )	range.min = 0;
		on_digit	do_( "{_" )	range.min = atoi(p);
		on_other	do_( "" )	status = regerr( "non-digit in '{}'" );
		end
		in_( "{_" ) bgn_
			on_( '}' )	do_( "{_}" )	if ( !range.min ) status = regerr( "range null" );
			on_digit	do_( same )
			on_( ',' )	do_( "{," )
			on_other	do_( "" )	status = regerr( "non-digit in '{}'" );
			end
		in_( "{," ) bgn_
			on_( '}' )	do_( "{_}" )	if ( !range.min ) status = regerr( "range unspecified" );
			on_digit	do_( "{,_" )	if ( range.min > atoi(p) ) status = regerr( "negative range" );
			on_other	do_( "" )	status = regerr( "non-digit in '{}'" );
			end
		in_( "{,_" ) bgn_
			on_( '}' )	do_( "{_}" )
			on_digit	do_( same )
			on_( ',' )	do_( "" )	status = regerr( "extraneous ',' in '{}'" );
			on_other	do_( "" )	status = regerr( "non-digit in '{}'" );
			end
		in_( "{_}" ) bgn_
			on_( '?' )	do_( "base" )
			on_( '*' )	do_( "base" )
			on_( '+' )	do_( "base" )
			on_other	do_( "base" )	REENTER
			end
	in_( "\\" ) bgn_
		on_any	do_( "_" )
		end
	in_( "[" ) bgn_
		on_( '^' )	do_( "[^" )
		on_( '[' )	do_( "" )	status = regerr( "'[' (unescaped) in alternative" );
		on_( ']' )	do_( "" )	status = regerr( "empty '[]'" );
		on_( '\\' )	do_( "[\\" )
		on_other	do_( "[_" )
		end
		in_( "[^" ) bgn_
			on_( ']' )	do_( "" )	status = regerr( "empty '[^]'" );
			on_( '\\' )	do_( "[\\" )
			on_other	do_( "[_" )
			end
		in_( "[\\" ) bgn_
			on_any	do_( "[_" )
			end
		in_( "[_" ) bgn_
			on_( '[' )	do_( "" )	status = regerr( "'[' (unescaped) in alternative" );
			on_( '\\' )	do_( "[\\" )
			on_( ']' )	do_( "_" )
			on_other	do_( same )
			end
	in_( "(" ) bgn_
		on_( '/' )	do_( "" )	status = regerr( "unclosed '('" );
		on_( '?' )	do_( "(?" )
		on_( '*' )	do_( "" )	status = regerr( "schema-leading '*' (unescaped)" );
		on_( '+' )	do_( "" )	status = regerr( "schema-leading '+' (unescaped)" );
		on_( '{' )	do_( "" )	status = regerr( "schema-leading '{' (unescaped)" );
		on_( '|' )	do_( "" )	status = regerr( "empty '(|'" );
		on_( ')' )	do_( "" )	status = regerr( "empty '()'" );
		on_other	do_( "base" )	REENTER
		end
		in_( "(?" ) bgn_
			on_( ':' )	do_( "(" )
			on_other	do_( "" )	status = regerr( "group-leading '?'" );
			end
	in_( "|" ) bgn_
		on_( '/' )	do_( "" )	status = regerr( "unclosed '('" );
		on_( '?' )	do_( "" )	status = regerr( "schema-leading '?' (unescaped)" );
		on_( '*' )	do_( "" )	status = regerr( "schema-leading '*' (unescaped)" );
		on_( '+' )	do_( "" )	status = regerr( "schema-leading '+' (unescaped)" );
		on_( '{' )	do_( "" )	status = regerr( "schema-leading '{' (unescaped)" );
		on_( '|' )	do_( "" )	status = regerr( "schema-leading '|' (unescaped)" );
		on_( ')' )	do_( "" )	status = regerr( "empty '|)'" );
		on_other	do_( "base" )	REENTER
		end
	CNStringStreamEnd

	RegexGroup *base = groups->ptr;
	if (( status < 0 ) || !(base->schemata)) {
		freeListItem( &stack );
		freePair((Pair *) base );
		freeListItem( &groups );
		return NULL;
	}
	reorderListItem( &groups );
	return groups;
}

static char *s_skid( char * );
static void
r_probe( Regex *r, int *passthrough )
/*
	passthrough is a flag used to track optionality of a
	group regardless of the group's suffix - that is, if
	any of the group's schema threads is a passthrough then
	the group as a whole is optional
*/
{
	*passthrough = 1;
	RegexGroup *base = (r->groups)->ptr;
	for ( char *p=base->position+1, *s; *p!='/'; ) {
		switch ( *p ) {
		case '(': p++;
			*passthrough = 1;
			break;
		case '|':
			if (!( *passthrough )) {
				p++; *passthrough = 1;
				break;
			}
			/* one of the schemas is a passthrough
			   it's enough for the whole group
			*/
			do {
				p++; s=post_p( p );
				p = s_skid( s );
			}
			while ( *p != ')' );
			// NO BREAK
		case ')':
			s = p + 1;
			if ( *passthrough || ( suffix_test( s, 0 ) == FORK_ON )) {
				*passthrough = 1;
				p = post_s( s );
			}
			else {
				*passthrough = 0;
				p = s_skid( s );
			}
			break;
		default:
			s = post_p( p );
			if ( suffix_test( s, 0 ) == FORK_ON )
				p = post_s( s );
			else {
				*passthrough = 0;
				p = s_skid( s );
			}
		}
	}
}
static char *
s_skid( char *s )
{
	char *p = post_s( s );
	while (( *p != '|' ) && ( *p != ')' ) && ( *p != '/' )) {
		s = post_p( p );
		p = post_s( s );
	}
	return p;
}

/*---------------------------------------------------------------------------
	RegexStatus
---------------------------------------------------------------------------*/
RegexRetval
RegexStatus( Regex *regex )
{
	return (((regex->frames )->ptr) || ((regex->frames)->next)) ?
		rvRegexEncore : rvRegexNoMore;
}

/*---------------------------------------------------------------------------
	freeRegex
---------------------------------------------------------------------------*/
static void r_update( Regex *, listItem ** );

void
freeRegex( Regex *regex )
{
	/* force failure - could also invoke RegexFrame( regex, EOF )
	   but here we make provision to support EOF as a stream event
	*/
	DECLARE( change );
	for ( listItem *i=regex->frames; i!=NULL; i=i->next ) {
		for ( listItem *j=i->ptr; j!=NULL; j=j->next ) {
			GroupThread *g = j->ptr;
			for ( listItem *k=g->s; k!=NULL; k=k->next ) {
				SchemaThread *s = k->ptr;
				if ( s->position[ 0 ] == '(' )
					continue;
				addItem( &change[ DEACTIVATE ], s );
			}
		}
	}
	r_update( regex, change );

	RegexRebase( regex, R_RESULTS );
	freeListItem( &regex->frames );
	for ( listItem *i=regex->groups; i!=NULL; i=i->next ) {
		RegexGroup *group = i->ptr;
		freeListItem( &group->schemata );
		freePair((Pair *) group );
	}
	freeListItem( &regex->groups );

	free( regex );
}

/*---------------------------------------------------------------------------
	RegexSeqFree, RegexRebase
---------------------------------------------------------------------------*/
void
RegexSeqFree( listItem **sequence )
{
	for ( listItem *i=*sequence; i!=NULL; i=i->next )
		freePair( i->ptr );
	freeListItem( sequence );
}

static SchemaThread s_base;
RegexRetval
RegexRebase( Regex *regex, int r_flags )
/*
	Note: parser does not invoke RegexRebase - as
	1. parser does not relaunch regex when it failed
	2. parser handles regex results directly
*/
{
	if ( r_flags & R_RESULTS ) {
		RegexSeqFree( &regex->results );
	}
	if ( r_flags & R_BASE ) {
		DECLARE( change );
		DBG_RX_REBASE;
		addItem( &change[ ACTIVATE ], &s_base );
		r_update( regex, change );
	}
	return RegexStatus( regex );
}

/*---------------------------------------------------------------------------
	RegexFrame
---------------------------------------------------------------------------*/
#define S_INHIBITED( s ) ( s->icount == -1 )
static void s_inhib( Regex *, SchemaThread * );
static int s_test( SchemaThread *, char * );

RegexRetval
RegexFrame( Regex *regex, int event, int capture )
{
	DECLARE( change );
	GroupThread *g;
	SchemaThread *s;

	int consumed = 0;
	DBG_RX_FRAME_START

	/* first, let all who release without consuming handle the event
	*/
	for ( listItem *i=regex->frames; i!=NULL; i=i->next ) {
		for ( listItem *j=i->ptr; j!=NULL; j=j->next ) {
			g = j->ptr;
			for ( listItem *k=g->s; k!=NULL; k=k->next ) {
				s = k->ptr;
				char *p = s->position;
				switch ( *p ) {
				case '/':
					/* regex is not a passthrough */
					addItem( &change[ RELEASE ], s );
					continue;
				case '|':
				case ')':
					addItem( &change[ PREPROCESS ], s );
					continue;
				}
				char *suffix = post_p( p );
				switch ( suffix_test( suffix, s->icount ) ) {
				case MOVE_ON:
				case REPEAT:
					continue;
				}
				s = s_fork( s );
				p = s->position = post_s( suffix );
				DBG_RX_position( s );
				addItem( &change[ PREPROCESS ], s );
			}
		}
	}
	listItem *completed = NULL;
	if (( change[ PREPROCESS ] )) do {
		SchemaThread *subscriber;
		reorderListItem( &change[ PREPROCESS ] );
		while (( s = popListItem( &change[ PREPROCESS ] ) )) {
			char *p = s->position, *suffix;
			switch ( *p ) {
			case '/':
				addItem( &change[ RELEASE ], s );
				continue;
			case '|':
			case ')':
				g = s->g;
				/* prevent sibling schemas from completing
				*/
				if (( lookupItem( completed, s->g ) )) {
					DBG_RX_OUT( s );
					s_free( s );
					if ( g->s == NULL ) g_free( regex, g );
					continue;
				}
				else addItem( &completed, s->g );

				/* move schema thread to subscriber's group
				*/
				subscriber = s->g->subscriber;
				s_collect( s, subscriber );
				removeItem( &s->g->s, s );
				s->g = subscriber->g;
				addItem( &s->g->s, s );

				/* advance schema thread from subscriber's position
				*/
				p = subscriber->position;
				suffix = post_p( p );
				p = s->position = post_s( suffix );
				switch ( *p ) {
				case '/':
				case '|':
				case ')':
					addItem( &change[ PREPROCESS ], s );
					continue;
				}
				break;
			case '(':
				addItem( &change[ ACTIVATE ], s );
				break;
			}
			suffix = post_p( p );
			switch ( suffix_test( suffix, s->icount ) ) {
			case MOVE_ON:
			case REPEAT:
				continue;
			}
			s = s_fork( s );
			s->position = post_s( suffix );
			DBG_RX_position( s );
			addItem( &change[ PREPROCESS ], s );
		}
		r_update( regex, change );
	}
	while (( change[ PREPROCESS ] ));
	freeListItem( &completed );

	DBG_RX_PREPROCESS_DONE

	/* then let all who consume before releasing process the event
	*/
	for ( listItem *i=regex->frames; i!=NULL; i=i->next ) {
		for ( listItem *j=i->ptr; j!=NULL; j=j->next ) {
			g = j->ptr;
			for ( listItem *k=g->s; k!=NULL; k=k->next ) {
				s = k->ptr;
				if ( S_INHIBITED( s ) ) {
					addItem( &change[ DEACTIVATE ], s );
					continue;
				}
				DBG_RX_expect( s );
				char *p = s->position;
				switch ( *p ) {
				case '/':
					DBG_RX_position( s );
					continue;
				case '(':
					DBG_RX_CR;
					continue;
				}
				if ( !p_test( p, event ) ) {
					DBG_RX_FAILED;
					addItem( &change[ DEACTIVATE ], s );
					continue;
				}
				consumed = 1;
				if ( capture ) s_register( regex, s, event );
				char *suffix = post_p( p );
				switch ( s_test( s, suffix ) ) {
				case MOVE_ON:
					p = s->position = post_s( suffix );
					DBG_RX_position( s );
					break;
				case REPEAT:
				case FORK_ON:
					DBG_RX_CR;
					continue;
				}
				switch ( *p ) {
				case '(':
					addItem( &change[ ACTIVATE ], s );
					break;
				case '|':
				case ')':
					for ( listItem *l=g->s; l!=NULL; l=l->next ) {
						SchemaThread *t = l->ptr;
						if ( t != s ) s_inhib( regex, t );
					}
					addItem( &change[ RELEASE ], s );
					break;
				}
			}
		}
	}
	/* setup next frame & propagate change
	*/
	if (( ( regex->frames )->ptr ))
		addItem( &regex->frames, NULL );

	r_update( regex, change );

	freeListItem( &change[ PREPROCESS ] );

	/* return status
	*/
	return ( consumed || ( regex->results )) ? RegexStatus( regex ) : rvRegexNoMatch;
}

static void
s_inhib( Regex *r, SchemaThread *s )
{
	listItem *inhibit = NULL;
	do {
		s->icount = -1;
		char *p = s->position;
		if ( *p == '(' ) {
			GroupThread *g = s->provider;
			for ( listItem *i=g->s; i!=NULL; i=i->next )
				addItem( &inhibit, i->ptr );
		}
	}
	while (( s = popListItem( &inhibit ) ));
}
static int
s_test( SchemaThread *s, char *suffix )
{
	if ( !is_suffix( suffix ) )
		return MOVE_ON;

	/* s_test must always look ahead in order to MOVE_ON
	   (provided p_test passes) upon the last preprocessing FORK_ON
	*/
	int test = suffix_test( suffix, s->icount + 1 );
	switch ( test ) {
	case REPEAT:
		s->icount++;
		break;
	case FORK_ON:
		switch ( *suffix ) {
		case '?':
		case '+':
			s->icount = 1;
			break;
		case '{':
			s->icount++;
			break;
		}
		break;
	case MOVE_ON:
		s->icount = 0;
		break;
	}
	return test;
}

/*---------------------------------------------------------------------------
	r_update
---------------------------------------------------------------------------*/
static int g_failed( GroupThread * );
static int last_s( GroupThread *, SchemaThread * );

static void
r_update( Regex *regex, listItem **change )
{
	RegexGroup *group;
	GroupThread *g;
	SchemaThread *s, *subscriber;
	do {
		/* Propagate failure
		*/
		while (( s = popListItem( &change[ DEACTIVATE ] ))) {
			DBG_RX_OUT( s );
			g = s->g;
			if ( !S_INHIBITED( s ) && (g->subscriber) && last_s( g, s ))
				addItem( &change[ DEACTIVATE ], g->subscriber );
			s_free( s );
			if ( g->s == NULL ) g_free( regex, g );
		}
		/* Propagate success
		*/
		while (( s = popListItem( &change[ RELEASE ] ) )) {
			if ( S_INHIBITED( s ) ) {
				addItem( &change[ DEACTIVATE ], s );
				continue;
			}
			g = s->g;
			if (( g->subscriber )) {
				subscriber = g->subscriber;
				if ( last_s( g, s ) ) DBG_RX_NFORK( subscriber );
				else subscriber = s_fork( subscriber );
				s_feed( s, subscriber );
				subscriber->provider = NULL;
				char *p = subscriber->position;
				char *suffix = post_p( p );
				switch ( s_test( subscriber, suffix ) ) {
				case MOVE_ON:
					p = subscriber->position = post_s( suffix );
					break;
				case REPEAT:
					break;
				case FORK_ON:
					addItem( &change[ ACTIVATE ], subscriber );
					subscriber = s_fork( subscriber );
					p = subscriber->position = post_s( suffix );
					break;
				}
				DBG_RX_position( subscriber );
				switch ( *p ) {
				case '(':
					addItem( &change[ ACTIVATE ], subscriber );
					break;
				case '|':
				case ')':
					for ( listItem *i=subscriber->g->s; i!=NULL; i=i->next ) {
						SchemaThread *t = i->ptr;
						if ( t != subscriber ) s_inhib( regex, t );
					}
					addItem( &change[ RELEASE ], subscriber );
				}
			}
			else if (( s->sequence )) {
				DBG_RX_FLUSH( s );
				if (!( regex->results )) {
					reorderListItem( &s->sequence );
					regex->results = s->sequence;
					s->sequence = NULL;
				}
				else WRN_RX_multiple_results(); // should not happen
			}
			DBG_RX_OUT(s)
			s_free( s );
			if ( g->s == NULL ) g_free( regex, g );
		}
		/* Propagate new subscriptions
		*/
		while (( subscriber = popListItem( &change[ ACTIVATE ] ) )) {
			if ( S_INHIBITED( subscriber ) ) {
				addItem( &change[ DEACTIVATE ], subscriber );
				continue;
			}
			if ( subscriber == &s_base ) {
				group = (regex->groups)->ptr;
				g_launch( regex, group, NULL, change );
			}
			else {
				group = find_group( regex, subscriber->position );
				g_launch( regex, group, subscriber, change );
			}
		}

	}
	while ( change[ DEACTIVATE ] || change[ RELEASE ] );
}

static int
last_s( GroupThread *g, SchemaThread *s )
{
	for ( listItem *i=g->s; i!=NULL; i=i->next ) {
		SchemaThread *t = i->ptr;
		if ( !S_INHIBITED( t ) && ( t != s ))
			return 0;
	}
	return 1;
}

/*---------------------------------------------------------------------------
	RegexExpand
---------------------------------------------------------------------------*/
static char *r_produce( char *format, int group_i, listItem *sequence );

listItem *
RegexExpand( Regex *r, listItem *strings, listItem *sequence )
{
	RegexGroup *base = (r->groups)->ptr;
	char *format = base->position + 1;

	listItem *values = NULL;
	CNString *string = newString();
	for ( listItem *j=strings; j!=NULL; j=j->next ) {
		for ( char *str=j->ptr; *str; str++ ) {
			if ( *str != '\\' ) {
				StringAppend( string, *str );
			}
			else if ( isdigit( str[ 1 ] ) && ( str[ 1 ] != '0' ) ) {
				/* extract referenced capture group
				*/
				int id = atoi( str + 1 );
				char *group = r_produce( format, id, sequence );
				if (( group )) {
					for ( char *g=group; *g; g++ )
						StringAppend( string, *g );
					free( group );
				}
				do { str++; } while ( isdigit( str[ 1 ] ) );
			}
			else {
				StringAppend( string, '\\' );
				if ( str[ 1 ] ) {
					StringAppend( string, str[ 1 ] );
					str++;
				}
			}
		}
		char *str = StringFinish( string, 0 );
		StringReset( string, CNStringMode );
		if (( str )) addItem( &values, str );
	}
	reorderListItem( &values );
	freeString( string );
	return values;
}
static char *
r_produce( char *format, int group_i, listItem *sequence )
{
	if (( sequence == NULL ) || ( group_i <= 0 ))
		return NULL;

	// find group's position range
	uintptr_t range[ 2 ] = { 0, 0 };
	int count = 0, quiet = 0;
	for ( char *fmt=format; (*fmt!='/') && !(range[ 1 ]); ) {
		switch ( *fmt ) {
		case '(':
			if ( quiet )
				quiet++;
			else if (( fmt[ 1 ] == '?' ) && ( fmt[ 2 ] == ':' ))
				{ quiet++; fmt += 2; }
			else if ( group_i == ++count )
				{ range[ 0 ] = (uintptr_t) fmt; }
			fmt++;
			break;
		case ')':
			if ( quiet ) quiet--;
			else if ( count-- == group_i )
				range[ 1 ] = (uintptr_t) fmt;
			fmt++;
			break;
		default:
			fmt = post_p( fmt );
			break;
		}
	}
	if (( range[ 0 ] == 0 ) || ( range[ 1 ] == 0 ))
		return NULL;

	CNString *string = newString();
	for ( listItem *i=sequence; i!=NULL; i=i->next ) {
		Pair *pair = i->ptr;
		uintptr_t p = (uintptr_t) pair->name;
		if ( p <= range[ 0 ] )
			continue;
		if ( p >= range[ 1 ] )
			break;
		StringAppend( string, (int) pair->value );
	}
	char *group = StringFinish( string, 0 );
	StringReset( string, CNStringMode );
	return group;
}

/*===========================================================================

	Regex threads private interface -- implementation

===========================================================================*/
static GroupThread *
g_launch( Regex *regex, RegexGroup *group, SchemaThread *subscriber, listItem **change )
{
	GroupThread *g = g_new( regex, group, subscriber );
	if ( g == NULL ) return NULL;

	for ( listItem *i=group->schemata; i!=NULL; i=i->next ) {
		RegexSchema *schema = i->ptr;
		SchemaThread *s = s_new( g, schema );
		if ( s == NULL ) break;

		char *p = s->position;
		DBG_RX_expect( s ); DBG_RX_CR;
		if ( *p == '(' )
			addItem( &change[ ACTIVATE ], s );
		char *suffix = post_p( p );
		switch ( suffix_test( suffix, 0 ) ) {
		case FORK_ON:
			s = s_fork( s );
			s->position = post_s( suffix );
			DBG_RX_position( s );
			addItem( &change[ PREPROCESS ], s );
			break;
		}
	}
	if (!(group->schemata)) {
		g_free( regex, g );
		g = NULL;
		addItem( &change[ DEACTIVATE ], subscriber );
	}
	return g;
}
static GroupThread *
g_new( Regex *regex, RegexGroup *group, SchemaThread *subscriber )
{
	GroupThread *g = calloc( 1, sizeof(GroupThread) );
	if ( g == NULL ) return NULL;
	g->group = group;
	g->f = regex->frames;
	addItem((listItem **) &g->f->ptr, g );
	if (( subscriber )) {
		g->subscriber = subscriber;
		subscriber->provider = g;
	}
	return g;
}
static void
g_free( Regex *regex, GroupThread *g )
{
	listItem *f = g->f;
	removeItem((listItem **) &f->ptr, g );
	if (( f->ptr == NULL ) && ( f != regex->frames )) {
		clipItem( &regex->frames, f );
	}
	free( g );
}
static SchemaThread *
s_new( GroupThread *g, RegexSchema *schema )
{
	SchemaThread *s = calloc( 1, sizeof(SchemaThread) );
	if ( s == NULL ) return NULL;
	s->schema = schema;
	s->position = schema;
	s->g = g;
	addItem( &g->s, s );
	return s;
}
static void
s_free( SchemaThread *s )
{
	if (( s->sequence ))
		RegexSeqFree( &s->sequence );
	removeItem( &s->g->s, s );
	free( s );
}
static Sequence *seqdup( Sequence * );
SchemaThread *
s_fork( SchemaThread *s )
{
	SchemaThread *clone = malloc( sizeof(SchemaThread) );
	if ( clone == NULL ) return NULL;
	clone->schema = s->schema;
	clone->position = s->position;
	clone->icount = s->icount;
	clone->provider = s->provider;
	clone->sequence = seqdup( s->sequence );
	clone->g = s->g;
	addItem( &s->g->s, clone );

	DBG_RX_fork( s, clone );
	return clone;
}
static Sequence *
seqdup( Sequence *sequence )
{
	if ( sequence == NULL ) return NULL;
	listItem *list = NULL;
	for ( listItem *i = sequence; i!=NULL; i=i->next ) {
		Pair *p1 = i->ptr;
		Pair *p2 = newPair( p1->name, p1->value );
		addItem( &list, p2 );
	}
	reorderListItem( &list );
	return list;
}
static void
s_register( Regex *r, SchemaThread *s, int event )
/*
	capture the event if the passed schema position is inside of
	a capture group - and not inside (?: ... )
*/
{
	RegexGroup *base = (r->groups)->ptr;
	char *position = s->position;

	uintptr_t range[ 2 ] = { 0, 0 };
	int count=0, found=0, quiet=0, capture=0;
	for ( char *p=base->position+1; (*p!='/') && !found; ) {
		switch ( *p ) {
		case '(':
			if ( quiet )
				quiet++;
			else if (( p[ 1 ] == '?' ) && ( p[ 2 ] == ':' ))
				{ quiet++; p += 2; }
			else count++;
			p++;
			break;
		case ')':
			if ( quiet ) quiet--;
			else count--;
			p++;
			break;
		default:
			if ( p == position ) {
				capture = !quiet && ( count > 0 );
				found = 1;
			}
			p = post_p( p );
			break;
		}
	}

	if ( capture ) {
		union { int value; void *ptr; } icast;
		icast.value = event;
		addItem( &s->sequence, newPair( position, icast.ptr ) );
	}
	else if (!( s->sequence )) {
		// if we leave the sequence empty then yak will fail
		// although sometimes we just want to skip event
		addItem( &s->sequence, newPair( NULL, NULL ) );
	}
}
static void
s_feed( SchemaThread *s, SchemaThread *subscriber )
{
	subscriber->sequence = catListItem( s->sequence, subscriber->sequence );
	s->sequence = NULL;
}
static void
s_collect( SchemaThread *s, SchemaThread *subscriber )
{
	Sequence *sequence = seqdup( subscriber->sequence );
	s->sequence = catListItem( sequence, s->sequence );
}

/*===========================================================================

	Regex scheme / format utilities

===========================================================================*/
static RegexGroup *
find_group( Regex *r, char *p )
{
	for ( listItem *i=r->groups; i!=NULL; i=i->next ) {
		RegexGroup *g = i->ptr;
		if ( g->position == p ) return g;
	}
	return NULL;
}

/*---------------------------------------------------------------------------
	navigation
---------------------------------------------------------------------------*/
static char *
post_1p( char *p )
{
	switch ( *p ) {
	case '/':
		break;
	case '[': p++;
		for ( int count=1; count; ) {
			switch ( *p ) {
			case '\0':	// impossible
			case '[': p++; count++; break;
			case ']': p++; count--; break;
			case '\\':
				if ( !p[ 1 ] ) return p + 1;
				p += 2;
				break;
			default:
				p++;
			}
		}
		break;
	case '\\':
		if ( !p[ 1 ] ) return p + 1;
		p += 2;
		break;
	default:
		p++;
	}
	return p;
}
static char *
post_p( char *p )
{
	int count = 0;
	do {
		switch ( *p ) {
		case '(': p++; count++; break;
		case ')': p++; count--; break;
		default:
			p = post_1p( p );
		}
	}
	while ( count );
	return p;
}
static char *
post_s( char *s )
{
	/* return post-suffix position
	*/
	do {
		switch ( *s ) {
		case '\0':	// impossible
		case '/': return s;
		case '?':
		case '*':
		case '+': s++; break;
		case '{': s++;
			for ( int done=0; !done; ) {
				switch ( *s ) {
				case '\0':	// impossible
				case '/': return s;
				case '}': s++; done = 1; break;
				default:
					s++;
				}
			}
			break;
		}
	}
	while ( is_suffix( s ) );
	return s;
}
static int
is_suffix( char *s )
{
	switch ( *s ) {
	case '?':
	case '*':
	case '+':
	case '{': return 1;
	}
	return 0;
}

/*---------------------------------------------------------------------------
	testing
---------------------------------------------------------------------------*/
static int escaped( int c );
static int in_range( char **, int event );

static int
p_test( char *p, int event )
{
	int expected, not = 0;
	switch ( *p ) {
	case '[': p++;
		if ( *p == '^' ) { p++; not = 1; }
		for ( int done=0; !done; ) {
			switch ( *p ) {
			case '\0':
				expected = ( not ? event : !event );
				done = 1;
				break;
			case ']':
				expected = ( not ? event : !event );
				done = 1;
				break;
			case '\\':
				if ( p[ 1 ] == (char) 0 ) {
					expected = ( not ? event : !event );
					done = 1;
				}
				else if ( p[ 2 ] == '-' ) {
					if ( in_range( &p, event ) ) {
						expected = ( not ? !event : event );
						done = 1;
					}
				}
				else if ( event == escaped( p[ 1 ] ) ) {
					expected = ( not ? !event : event );
					done = 1;
				}
				else p += 2;
				break;
			default:
				if ( p[ 1 ] == '-' ) {
					if ( in_range( &p, event ) ) {
						expected = ( not ? !event : event );
						done = 1;
					}
				}
				else if ( event == *p ) {
					expected = ( not ? !event : event );
					done = 1;
				}
				else p++;
			}
		}
		break;
	case '\\':
		expected = escaped( p[ 1 ] );
		break;
	case '.':
		expected = event;
		break;
	default:
		expected = *p;
	}
	return ( event == expected );
}
static int
escaped( int c )
{
	switch ( c ) {
		case '0': return '\0';
		case 'n': return '\n';
		case 't': return '\t';
		default : return c;
	}
}
static int
in_range( char **p, int event )
{
	struct { int min, max; } range;
	char *s = *p;
	if ( s[ 0 ] == '\\' ) {
		range.min = escaped( s[ 1 ] );
		if ( s[ 3 ] == '\\' ) {
			if ( s[ 4 ] == (char) 0 ) {
				*p = s + 4;
				return 0;
			}
			else {
				range.max = escaped( s[ 4 ] );
				*p = s + 5;
			}
		}
		else if ( s[ 3 ] == (char) 0 ) {
			*p = s + 3;
			return 0;
		}
		else {
			range.max = s[ 3 ];
			*p = s + 4;
		}
	}
	else {
		range.min = s[ 0 ];
		if ( s[ 2 ] == '\\' ) {
			if ( s[ 3 ] == (char) 0 ) {
				*p = s + 3;
				return 0;
			}
			else {
				range.max = escaped( s[ 3 ] );
				*p = s + 4;
			}
		}
		else if ( s[ 2 ] == (char) 0 ) {
			*p = s + 2;
			return 0;
		}
		else {
			range.max = s[ 2 ];
			*p = s + 3;
		}
	}
	return (( event >= range.min ) && ( event <= range.max ));
}

static int
suffix_test( char *s, int icount )
{
	switch ( *s ) {
	case '?':
		return icount ? MOVE_ON : FORK_ON;
	case '*':
		return FORK_ON;
	case '+':
		return icount ? FORK_ON : REPEAT;
	case '{':
		s++;
		struct { int min, max; } range = { 0, 0 };
		range.min = atoi( s );
		while (( *s != ',' ) && ( *s != '}' )) s++;
		switch ( *s ) {
		case '}': s++; range.max = range.min; break;
		case ',': s++; range.max = atoi( s );
			while ( *s != '}' ) s++;
			s++;
		}
		int delta = range.max - range.min;
		switch ( *s ) {
		case '*':
		case '+':
			if ( icount == 0 )
				return (( *s == '*' ) || !range.min ) ? FORK_ON : REPEAT;
			else if ( !range.min )
				return ( icount % range.max ) ? REPEAT : FORK_ON;
			else if ( icount < range.min )
				return REPEAT;
			else if ( !delta )
				return ( icount - range.min ) % range.min ? REPEAT : FORK_ON;
			else if ( !range.max )
				return ( icount % range.min ) ? REPEAT : FORK_ON;
			else if ( ( icount - range.min ) % range.min < delta )
				return FORK_ON;
			else
				return REPEAT;
			break;
		case '?':
		default:
			if ( icount == 0 )
				return (( *s == '?' ) || !range.min ) ? FORK_ON : REPEAT;
			else if ( !range.min )
				return ( icount < range.max ) ? FORK_ON : MOVE_ON;
			else if ( icount < range.min )
				return REPEAT;
			else if ( !range.max )
				return FORK_ON;
			else if ( icount - range.min < delta )
				return FORK_ON;
			else
				return MOVE_ON;
		}
	}
	return MOVE_ON;
}

