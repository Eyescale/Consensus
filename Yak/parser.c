#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "parser_util.h"
#include "parser_debug.h"

#define REGEX
#ifdef REGEX
#include "regex.h"
#endif

/*===========================================================================

	Parser public interface -- implementation

===========================================================================*/
static void
ERR_rule_not_found( Rule *r, char *identifier )
{
	if ( r == NULL ) {
		fprintf( stderr, "Error: ParserValidate: no base rule\n" );
		return;
	}
	fprintf( stderr, "Error: ParserValidate: in rule '%s', rule not found: %%%s\n",
		r->identifier, identifier );
}
static void
ERR_cyclic_rule( Rule *r )
	{ fprintf( stderr, "Error: ParserValidate: cyclic rule '%s'\n", r->identifier ); }
static void
ERR_parser_defunct( void )
	{ fprintf( stderr, "Error: parser defunct\n" ); }

#ifdef REGEX
static void
WRN_regex_invalid( void )
	{ fprintf( stderr, "Warning: regex invalid - deactivating subscriber\n" ); }
#endif

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
	ParserValidate
---------------------------------------------------------------------------*/
static int r_cyclic( Scheme *, Rule *, listItem ** );

int
ParserValidate( Scheme *scheme )
{
	if ( scheme == NULL ) return 0;
	if ( SchemeBase( scheme ) == NULL ) {
		ERR_rule_not_found( NULL, NULL );
		return 0;
	}
	/* Ensure all rules referenced by schemas do in fact exist in scheme
	*/
	for ( listItem *i=scheme->rules; i!=NULL; i=i->next ) {
		Rule *r = i->ptr;
		for ( listItem *j=r->schemata; j!=NULL; j=j->next ) {
			Schema *schema = j->ptr;
			char *p;
			for ( s_init( &p, schema ); *p; s_advance( &p, schema ) ) {
				if ( p[ 0 ] != '%' ) continue;
				if (( r_inbuilt( p + 1 ) ? fetch_inbuilt( p + 1 ) :
					fetchRule( scheme, p ) ))
					continue;

				char *identifier;
				strscanid( p, &identifier );
				ERR_rule_not_found( r, identifier );
				free( identifier );
				return 0;
			}
		}
	}
	/* Detect cyclicity: a rule is cyclic if at least one of its schemas depends
	   solely on rules whose schema threads are not pending on any event.
	*/
	listItem *tagged[ 2 ] = { NULL, NULL };
	for ( listItem *i=scheme->rules; i!=NULL; i=i->next ) {
		Rule *r = i->ptr;
		if ( !r_cyclic( scheme, r, tagged ) ) {
			freeListItem( &tagged[ 1 ] );
			continue;
		}
		ERR_cyclic_rule( r );
		freeListItem( &tagged[ 0 ] );
		freeListItem( &tagged[ 1 ] );
		return 0;
	}
	freeListItem( &tagged[ 0 ] );
	freeListItem( &tagged[ 1 ] );

	return 1;
}

static int
r_cyclic( Scheme *scheme, Rule *r, listItem **tagged )
{
	/* tagged[ 0 ] holds previously identified non-cyclic rules
	   tagged[ 1 ] holds our current rule traversal path(s)
	*/
	if (( lookupIfThere( tagged[ 0 ], r ) ))
		return 0;
	if (( lookupItem( tagged[ 1 ], r ) ))
		return 1;
	addItem( &tagged[ 1 ], r );

	/* first detect if r does have a null-schema
	*/
	int ns = 0;
	for ( listItem *i=r->schemata; i!=NULL; i=i->next ) {
		Schema *schema = i->ptr;
		if ( s_null( schema ) ) { ns = 1; break; }
	}
	if ( ns && ( r->schemata->next == NULL ) )
		goto RETURN_NCYCLIC; // standalone null-schema

	/* then determine if r is cyclic
	   r is cyclic if at least one of its schemas depends solely on
	   rules whose schema threads are not pending on any event.
	   Here we assume cyclicity until proven otherwise.
	*/
	int tag = 0;
	for ( listItem *i=r->schemata; i!=NULL; i=i->next ) {
		Schema *schema = i->ptr;
		if ( s_null( schema ) ) continue;

		char *p;
		for ( s_init( &p, schema ); ; s_advance( &p, schema ) ) {
			if ( p[ 0 ] == ' ' )
				continue;
			else if ( !p[ 0 ] )
				return 1;
			else if (( p[ 0 ] != '%' ) || r_inbuilt( p + 1 ) )
				break;
			else {
				Rule *rule = fetchRule( scheme, p );
				if ( !r_cyclic( scheme, rule, tagged ) )
					break;
				else if ( rule != r )
					return 1;
				else if ( !ns )
					tag++; // schema depends on r
			}
		}
	}
	for ( listItem *i=r->schemata; i!=NULL; i=i->next )
		if ( !tag-- ) goto RETURN_NCYCLIC;
	return 1; // all of r's schema's depend on r to complete

RETURN_NCYCLIC:
	addIfNotThere( &tagged[ 0 ], r );
	return 0;
}

/*---------------------------------------------------------------------------
	newParser
---------------------------------------------------------------------------*/
/*
	Assumption: the scheme has been validated with respect to
	this parser implementation - cf main.c the user is responsible
	for calling ParserValidate( scheme ), cf main.c
*/
Parser *
newParser( Scheme *scheme, int options )
{
	if ( !ParserValidate( scheme ) )
		return NULL;

	Parser *parser = calloc( 1, sizeof(Parser) );
	if ( parser == NULL ) return NULL;
	parser->options = options;
	parser->scheme = scheme;
	addItem( &parser->frames, NULL );

	switch ( ParserRebase( parser, P_BASE ) ) {
	case rvParserNoMore:
		ERR_parser_defunct();
		freeParser( parser );
		parser = NULL;
		break;
	default:
		break;
	}	
	return parser;
}

/*---------------------------------------------------------------------------
	ParserStatus
---------------------------------------------------------------------------*/
ParserRetval
ParserStatus( Parser *parser )
{
	return (((parser->frames )->ptr) || ((parser->frames)->next)) ?
		rvParserEncore : rvParserNoMore;
}

/*---------------------------------------------------------------------------
	freeParser
---------------------------------------------------------------------------*/
static void p_update( Parser *, listItem ** );

void
freeParser( Parser *parser )
{
	/* force failure - could also invoke ParserFrame( parser, EOF )
	   but here we make provision to support EOF as a stream event
	*/
	DECLARE( change );
	for ( listItem *i=parser->frames; i!=NULL; i=i->next ) {
		for ( listItem *j=i->ptr; j!=NULL; j=j->next ) {
			RuleThread *r = j->ptr;
			for ( listItem *k=r->s; k!=NULL; k=k->next ) {
				SchemaThread *s = k->ptr;
				switch ( s->position[ 0 ] ) {
				case ' ':
				case '%':
					break;
				default:
					addItem( &change[ DEACTIVATE ], s );
				}
			}
		}
	}
	p_update( parser, change );

	ParserRebase( parser, P_DATA | P_RESULTS );
	freeListItem( &parser->frames );
	free( parser );
}

/*---------------------------------------------------------------------------
	ParserRebase
---------------------------------------------------------------------------*/
static void seqfree( Parser *, listItem ** );
static SchemaThread s_base;

ParserRetval
ParserRebase( Parser *parser, int p_flags )
{
	if ( p_flags & P_DATA ) {
		for ( listItem *i=parser->tokens; i!=NULL; i=i->next ) {
			for ( listItem *j=i->ptr; j!=NULL; j=j->next ) {
				free( j->ptr );
			}
			freeListItem(( listItem ** ) &i->ptr );
		}
		freeListItem( &parser->tokens );
	}
	if ( p_flags & P_RESULTS ) {
		for ( listItem *i=parser->results; i!=NULL; i=i->next ) {
			listItem *sequence = i->ptr;
			seqfree( parser, &sequence );
		}
		freeListItem( &parser->results );
	}
	if ( p_flags & P_BASE ) {
		DECLARE( change );
		DBG_REBASE;
		addItem( &change[ ACTIVATE ], &s_base );
		p_update( parser, change );
	}
	return ParserStatus( parser );
}

/*---------------------------------------------------------------------------
	ParserFrame
---------------------------------------------------------------------------*/
static void s_check( SchemaThread *s, listItem **change );

ParserRetval
ParserFrame( Parser *parser, int event )
{
	DECLARE( change );
	RuleThread *r;
	SchemaThread *s;

	DBG_event( event );
	DBG_frameno( parser->frames );
	int consumed = 0;

	/* first, let all who release without consuming handle the event
	   Assumption: no built-in function consumes before releasing
	*/
	for ( listItem *i=parser->frames; i!=NULL; i=i->next ) {
		for ( listItem *j=i->ptr; j!=NULL; j=j->next ) {
			r = j->ptr;
			if ( r_inbuilt( r->rule->identifier ) ) {
				for ( listItem *k=r->s; k!=NULL; k=k->next )
					addItem( &change[ PREPROCESS ], k->ptr );
			}
			else for ( listItem *k=r->s; k!=NULL; k=k->next ) {
				s = k->ptr;
				if ( s_null( s->schema ) )
					addItem( &change[ PREPROCESS ], s );
			}
		}
	}
	if (( change[ PREPROCESS ] )) do {
		while (( s = popListItem( &change[ PREPROCESS ] ) )) {
			char *r_id = s->r->rule->identifier;
			if ( r_inbuilt( r_id ) ) {
				switch ( *r_id ) {
#ifdef REGEX
				case '/': ;
					Regex *regex = (s->schema)->data;
					int capture = ( parser->options & P_TOK );
					int status = RegexFrame( regex, event, capture );
					DBG_expect( s );
					switch ( status ) {
					case rvRegexOutOfMemory:
						DBG_FAILED;
						addItem( &change[ DEACTIVATE ], s );
						break;
					case rvRegexNoMatch:
						/* Regex failed */
						if (( s->sequence )) {
							DBG_position( s );
							addItem( &change[ RELEASE ], s );
						}
						else {
							DBG_FAILED;
							addItem( &change[ DEACTIVATE ], s );
						}
						break;
					case rvRegexEncore:
						/* Regex has remaining active threads and/or results */
						DBG_CR;
						consumed = 1;
						if ( capture ) {
							RegexSeqFree( &s->sequence );
							s->sequence = regex->results;
							regex->results = NULL;
						}
						else s_register( parser, s, event );
						SchemaThread *subscriber = s->r->subscribers->ptr;
						subscriber = s_fork( parser, subscriber );
						s_feed( parser, s, subscriber );
						s_advance( &subscriber->position, subscriber->schema );
						s_register_advance( parser, subscriber );
						s_check( subscriber, change );
						break;
					case rvRegexNoMore:
						/* Regex completed and it has results
						   This happens when regex is not a passthrough
						   Note that there is a one-frame latency between
						   completion and notification, so that the event
						   is not consumed
						*/
						DBG_position( s );
						if ( capture ) {
							RegexSeqFree( &s->sequence );
							s->sequence = regex->results;
							regex->results = NULL;
						}
						addItem( &change[ RELEASE ], s );
						break;
					}
					break;
#endif
				case '-':
					DBG_expect( s );
					if ( ParserSpace( event ) ) {
						DBG_FAILED;
						addItem( &change[ DEACTIVATE ], s );
					}
					else {
						DBG_position( s );
						addItem( &change[ RELEASE ], s );
					}
					break;
				case '%':
					DBG_expect( s );
					if ( !ParserSeparator( event ) ) {
						DBG_CR;
						consumed = 1;
						s_register( parser, s, event );
					}
					else if (( s->sequence )) {
						DBG_position( s );
						addItem( &change[ RELEASE ], s );
					}
					else {
						DBG_FAILED;
						addItem( &change[ DEACTIVATE ], s );
					}
					break;
				case ' ':
					DBG_expect( s );
					if ( ParserSpace( event ) ) {
						DBG_CR;
						consumed = 1;
						if ( parser->options & P_SPACE )
							s_register( parser, s, event );
						else if ( s->sequence == NULL )
							s->sequence = newItem( NULL );
					}
					else if (( s->sequence )) {
						DBG_position( s );
						if ( !( parser->options & P_SPACE ) )
							freeListItem( &s->sequence );
						addItem( &change[ RELEASE ], s );
					}
					else {
						DBG_FAILED;
						addItem( &change[ DEACTIVATE ], s );
					}
					break;
				}
			}
			else if ( s_null( s->schema ) ) {
				DBG_expect( s ); DBG_position( s );
				addItem( &change[ RELEASE ], s );
			}
		}
		/* here we release the threads which were pending on built-in
		   rules. this may activate new built-in rules, whose threads
		   must in turn process the event - hence the outer do-while
		*/
		p_update( parser, change );
	}
	while (( change[ PREPROCESS ] ));

	/* then let all who consume before releasing process the event
	*/
	for ( listItem *i=parser->frames; i!=NULL; i=i->next ) {
		for ( listItem *j=i->ptr; j!=NULL; j=j->next ) {
			r = j->ptr;
			if ( r_inbuilt( r->rule->identifier ) )
				continue;
			for ( listItem *k=r->s; k!=NULL; k=k->next ) {
				s = k->ptr;
				if ( s_null( s->schema ) )
					continue;

				DBG_expect( s );
				int expected;
				char *position = s->position;
				switch ( position[ 0 ] ) {
					case '\0':
					case '%':
					case ' ':
						DBG_CR;
						continue;
					case '\\':
						switch ( position[ 1 ] ) {
							case '0': expected = '\0'; break;
							case 't': expected = '\t'; break;
							case 'n': expected = '\n'; break;
							default: expected = position[ 1 ];
						}
						break;
					default:
						expected = position[ 0 ];
				}
				if ( event == expected ) {
					consumed = 1;
					s_advance( &s->position, s->schema );
					s_register( parser, s, event );
					s_check( s, change );
				}
				else {
					DBG_FAILED;
					addItem( &change[ DEACTIVATE ], s );
				}
			}
		}
	}
	/* setup next frame & propagate change
	*/
	if (( ( parser->frames )->ptr ))
		addItem( &parser->frames, NULL );

	p_update( parser, change );

	freeListItem( &change[ PREPROCESS ] );

	/* return status
	*/
	return ( consumed || ( parser->results )) ? ParserStatus( parser ) : rvParserNoMatch;
}

static void
s_check( SchemaThread *s, listItem **change )
{
	DBG_position( s );
	switch ( s->position[ 0 ] ) {
	case '%':
	case ' ':
		addItem( &change[ ACTIVATE ], s );
		break;
	case '\0':
		addItem( &change[ RELEASE ], s );
		break;
	}
}

/*---------------------------------------------------------------------------
	p_update
---------------------------------------------------------------------------*/
static int r_failed( RuleThread * );
static void r_register_push( Parser *, RuleThread * );

static void
p_update( Parser *parser, listItem **change )
{
	Rule *rule;
	RuleThread *r;
	SchemaThread *s, *subscriber;
	do {
		/* Propagate failure
		*/
		while (( s = popListItem( &change[ DEACTIVATE ] ))) {
			DBG_OUT( s );
			r = s->r;
			s_free( parser, s );
			if ( r->subscribers ) {
				/* when r has no schema thread left besides possibly
				   those pending on r itself to complete, then we must
				   propagate failure to all of r's subscribers
				*/
				removeItem( &r->subscribers, s );
				if ( r_failed( r ) ) {
					for ( listItem *j=r->subscribers; j!=NULL; j=j->next )
						addItem( &change[ DEACTIVATE ], j->ptr );
					freeListItem( &r->subscribers );
				}
			}
			if ( r->s == NULL ) r_free( parser, r );
		}

		/* Propagate success
		*/
		while (( s = popListItem( &change[ RELEASE ] ) )) {
			r = s->r;
			s_register_pop( parser, s );
			if (( r->subscribers )) {
				int last_s = ( r->s->next == NULL );
				for ( listItem *i=r->subscribers; i!=NULL; i=i->next ) {
					subscriber = i->ptr;
					/* when r does have other schema threads running
					   then we fork each of r's subscriber - keeping
					   the originals pending on future results.
					*/
					if ( last_s ) DBG_NFORK( subscriber );
					else subscriber = s_fork( parser, subscriber );
					s_feed( parser, s, subscriber );
					s_advance( &subscriber->position, subscriber->schema );
					s_register_advance( parser, subscriber );
					s_check( subscriber, change );
				}
				if ( last_s ) freeListItem( &r->subscribers );
			}
			else if (( s->sequence )) {
				DBG_FLUSH( s );
				/* For each base rule, ie. those without subscribers,
				   upon completion we send back intermediate results.
				   The user shall decide how s/he wants to use them.
				*/
				reorderListItem( &s->sequence );
				addItem( &parser->results, s->sequence );
				s->sequence = NULL;
			}
			DBG_OUT(s)
			s_free( parser, s );
			if ( r->s == NULL ) r_free( parser, r );
		}

		/* Propagate new subscriptions
		*/
		while (( subscriber = popListItem( &change[ ACTIVATE ] ) )) {
			if ( subscriber == &s_base ) {
				rule = SchemeBase( parser->scheme );
				r = r_launch( parser, rule, NULL, change );
				if (( r )) r_register_push( parser, r );
			}
			else if ( subscriber->position[ 0 ] == ' ' ) {
				r_launch_space( parser, subscriber, change );
			}
			else if ( r_inbuilt( subscriber->position + 1 ) ) {
				r_launch_inbuilt( parser, subscriber, change );
			}
			else {
				rule = fetchRule( parser->scheme, subscriber->position );
				r = r_launch( parser, rule, subscriber, change );
				if (( r )) r_register_push( parser, r );
			}
		}

	}
	while ( change[ DEACTIVATE ] || change[ RELEASE ] );
}

static int
r_failed( RuleThread *r )
{
	if ( r->subscribers == NULL )
		return 0; // no subscriber to notify

	for ( listItem *i=r->s; i!=NULL; i=i->next ) {
		SchemaThread *s = i->ptr;
		if (( lookupItem( r->subscribers, s ) ))
			continue;
		return 0; // not failed
	}
	return 1; // failed
}
static void
r_register_push( Parser *p, RuleThread *r )
{
	if ( r == NULL ) return;
	for ( listItem *i=r->s; i!=NULL; i=i->next )
		s_register_push( p, i->ptr );
}

/*---------------------------------------------------------------------------
	r_launch
---------------------------------------------------------------------------*/
static RuleThread *r_launch_( Parser *, Rule *, SchemaThread *subscriber );

static RuleThread *
r_launch( Parser *parser, Rule *rule, SchemaThread *subscriber, listItem **change )
{
	RuleThread *r = r_launch_( parser, rule, subscriber );
	if ( r == NULL ) return NULL;

	for ( listItem *i=rule->schemata; i!=NULL; i=i->next ) {
		Schema *schema = i->ptr;
		SchemaThread *s = s_new( r, schema );
		if ( s == NULL ) break;

		s_init( &s->position, schema );
		DBG_expect( s ); DBG_CR;
		switch ( s->position[ 0 ] ) {
		case '%':
		case ' ':
			addItem( &change[ ACTIVATE ], s );
			break;
		case '\0':
			addItem( &change[ PREPROCESS ], s );
			break;
		}
	}
	return r;
}

static RuleThread *
r_launch_( Parser *parser, Rule *rule, SchemaThread *subscriber )
{
	/* r is only created if this rule is new to this frame
	   subscriber is only added if new to r
	*/
	RuleThread *r;
	for ( listItem *i=(parser->frames)->ptr; i!=NULL; i=i->next ) {
		r = i->ptr;
		if ( r->rule != rule ) continue;
		/* rule already active in this frame
		*/
		// DBG_rule_found( rule->identifier );
		/* add subscriber if it is new to r
		*/
		if (( subscriber ) && !( lookupItem( r->subscribers, subscriber ) ))
			addItem( &r->subscribers, subscriber );
		return NULL;
	}
	r = r_new( parser, rule, subscriber );
	return r;
}

/*===========================================================================

	Parser built-in rules private interface -- implementation

===========================================================================*/
static Rule r_builtin[] = {
	{ "/", NULL },	// built-in regex rule
	{ "-", NULL },	// built-in hyphen rule
	{ "%", NULL },	// built-in identifier rule
	{ " ", NULL },	// built-in space rule
	{ NULL, NULL }	// terminator
};

static int
r_inbuilt( char *id )
{
	return ( *id && ParserSeparator( *id ) );
}
static Rule *
fetch_inbuilt( char *id )
{
	for ( Rule *r=r_builtin; r->identifier!=NULL; r++ ) {
		if ( r->identifier[ 0 ] == *id )
			return r;
	}
	return NULL;
}

/*
	s_null(), s_init() and s_advance() are dependent on s_next()
	which is inbuilt-implementation specific
*/
static char *s_next( char *position, Schema * );
static int
s_null( Schema *schema )
{
	char *position = s_next( NULL, schema );
	return ( position[ 0 ] == (char) 0 );
}
static void
s_init( char **position, Schema *schema )
{
	*position = s_next( NULL, schema );
}
static void
s_advance( char **position, Schema *schema )
{
	*position = s_next( *position, schema );
}
static char *
s_next( char *p, Schema *schema )
{  
	if ( p == NULL ) {
		p = schema->string;
	}
	else switch ( p[ 0 ] ) {
		case '\0':
			return p;
		case '%':
#ifdef REGEX
			if ( p[ 1 ] == '/' ) {
				p += 2;
				for ( int done=0; *p && !done; ) {
					switch ( *p ) {
					case '\\': p += 2; break;
					case '/' : p++; done = 1; break;
					default: p++;
					}
				}
			} else
#endif
			if ( r_inbuilt( p + 1 ) )
				p += 2;
			else
				p = strskip( ".", p + 1 );
			break;
		case '\\':
			p += 2;
			break;
		default:
			p++;
			break;
	}
	while (( p[ 0 ] == '%' ) && ( p[ 1 ] == '-' )) p += 2;
	return p;
}

/*---------------------------------------------------------------------------
	r_launch_inbuilt
---------------------------------------------------------------------------*/
static Schema null_s = { "", NULL };

static RuleThread *
r_launch_inbuilt( Parser *parser, SchemaThread *subscriber, listItem **change )
{
	char *r_id = subscriber->position + 1;
	RuleThread *r;
#ifdef REGEX
	if ( *r_id == '/' ) {
		int passthrough;
		Regex *regex = newRegex( r_id, &passthrough );
		if ( regex == NULL ) {
			WRN_regex_invalid();
			if ((subscriber)) addItem( &change[ DEACTIVATE ], subscriber );
			return NULL;
		}
		if ( passthrough ) {
			SchemaThread *clone = s_fork( parser, subscriber );
			s_advance( &clone->position, clone->schema );
			s_register_advance( parser, clone );
			s_check( clone, change );
		}
		r = r_new( parser, fetch_inbuilt( "/" ), subscriber );
		Schema *regex_s = (Schema *) newPair( "", regex );
		SchemaThread *s = s_new( r, regex_s );
		addItem( &change[ PREPROCESS ], s );
	}
	else
#endif
	{
		r = r_launch_( parser, fetch_inbuilt( r_id ), subscriber );
		if (( r )) {
			SchemaThread *s = s_new( r, &null_s );
			addItem( &change[ PREPROCESS ], s );
		}
	}
	return r;
}

/*---------------------------------------------------------------------------
	r_launch_space
---------------------------------------------------------------------------*/
static RuleThread *
r_launch_space( Parser *parser, SchemaThread *subscriber, listItem **change )
{
	RuleThread *r = r_launch_( parser, fetch_inbuilt(" "), subscriber );
	if (( r )) {
		SchemaThread *s = s_new( r, &null_s );
		addItem( &change[ PREPROCESS ], s );
	}
	subscriber = s_fork( parser, subscriber );
	do {
		s_advance( &subscriber->position, subscriber->schema );
		s_register_advance( parser, subscriber );
	}
	while ( subscriber->position[ 0 ] == ' ' );
	s_check( subscriber, change );

	return r;
}

/*===========================================================================

	Parser threads private interface -- implementation

===========================================================================*/
static RuleThread *
r_new( Parser *parser, Rule *rule, SchemaThread *subscriber )
{
	RuleThread *r = calloc( 1, sizeof(RuleThread) );
	if ( r == NULL ) return NULL;
	r->rule = rule;
	r->f = parser->frames;
	addItem((listItem **) &r->f->ptr, r );
	if (( subscriber )) addItem( &r->subscribers, subscriber );
	return r;
}
static void
r_free( Parser *parser, RuleThread *r )
{
	freeListItem( &r->subscribers );
	listItem *f = r->f;
	removeItem((listItem **) &f->ptr, r );
	if (( f->ptr == NULL ) && ( f != parser->frames )) {
		clipItem( &parser->frames, f );
	}
	free( r );
}
static SchemaThread *
s_new( RuleThread *r, Schema *schema )
{
	SchemaThread *s = calloc( 1, sizeof(SchemaThread) );
	if ( s == NULL ) return NULL;
	s->schema = schema;
	s->position = schema->string;
	s->r = r;
	addItem( &r->s, s );
	return s;
}
static void seqfree( Parser *, listItem ** );
static void
s_free( Parser *p, SchemaThread *s )
{
	seqfree( p, &s->sequence );
#ifdef REGEX
	char *r_id = s->r->rule->identifier;
	if ( *r_id == '/' ) {
		Schema *schema = s->schema;
		freeRegex(( Regex *) schema->data );
		freePair(( Pair *) schema );
	}
#endif
	removeItem( &s->r->s, s );
	free( s );
}
static void
seqfree( Parser *p, listItem **sequence )
{
	if ( !( p->options & P_TOK ) ) {
		/* free s->sequence - non-recursive
		*/
		CNStreamBegin( void *, stream, *sequence )
		CNOnStreamValue( stream, Pair *, pair )
			if (( pair->name )) {
				CNStreamPush( stream, pair->value );
			}
			else {
				freePair( pair );
				CNStreamFlow( stream );
			}
		CNOnStreamEnd
			CNInStreamPop( stream, Pair *, pair )
				freeListItem((listItem **) &pair->value );
				freePair( pair );
		CNStreamEnd( stream )
	}
	freeListItem( sequence );
}
static Sequence *seqdup( Parser *, Sequence * );
SchemaThread *
s_fork( Parser *p, SchemaThread *s )
{
	SchemaThread *clone = malloc( sizeof(SchemaThread) );
	if ( clone == NULL ) return NULL;
	clone->schema = s->schema;
	clone->position = s->position;
	clone->sequence = seqdup( p, s->sequence );
	clone->r = s->r;
	addItem( &s->r->s, clone );

	DBG_fork( s, clone );
	return clone;
}
static Sequence *
seqdup( Parser *p, Sequence *sequence )
{
	if ( sequence == NULL ) return NULL;
	if ( p->options & P_TOK ) {
		listItem *list = NULL;
		for ( listItem *i = sequence; i!=NULL; i=i->next )
			addItem( &list, i->ptr );
		reorderListItem( &list );
		return list;
	}
	else {
		CNStreamBegin( Sequence *, stream, sequence )
		CNOnStreamValue( stream, Pair *, pair )
			if (( pair->name )) {
				CNStreamPush( stream, pair->value );
			}
			else {
				pair = newPair( NULL, pair->value );
				addItem( &CNStreamRetour( stream ), pair );
				CNStreamFlow( stream );
			}
		CNOnStreamEnd
			reorderListItem( &CNStreamRetour( stream ) );
			CNInStreamPop( stream, Pair *, pair )
				pair = newPair( pair->name, CNStreamRetour( stream ) );
				addItem((Sequence **) &CNStreamReturn( stream ), pair );
		CNStreamEnd( stream )
		return CNStreamRetour( stream );
	}
}

static void
s_register_push( Parser *p, SchemaThread *s )
{
	if ( p->options & P_TOK ) {
		char *identifier = s->r->rule->identifier;
		if ( r_inbuilt( identifier ) )
			return;
		else if ( s_null( s->schema ) )
			return;
		else if ( !(s->sequence) ) {
			Schema *schema = s->schema;
			for ( listItem *i=schema->data; i!=NULL; i=i->next ) {
				Token *token = i->ptr;
				if (( token->position ))
					break;
				addItem( &s->sequence, token->values );
			}
		}
	}
}
static void
s_register( Parser *p, SchemaThread *s, int event )
{
	if ( p->options & P_TOK ) {
		char *r_id = s->r->rule->identifier;
#ifdef REGEX
		if ( *r_id == '/' )
			return;
		else
#endif
		if ( r_inbuilt( r_id ) ) {
			union { int value; void *ptr; } icast;
			icast.value = event;
			addItem( &s->sequence, icast.ptr );
		}
		else if ( s_null( s->schema ) ) {
			return;
		}
		else s_register_advance( p, s );
	}
	else {
		union { int value; void *ptr; } icast;
		icast.value = event;
		Pair *pair = newPair( NULL, icast.ptr );
		addItem( &s->sequence, pair );
	}
}
static void
s_register_advance( Parser *p, SchemaThread *s )
{
	/* Note: s_register and s_register_advance are always called post s_advance
	*/
	if ( p->options & P_TOK ) {
		Schema *schema = s->schema;
		uintptr_t position = (uintptr_t) s->position;
		for ( listItem *i=schema->data; i!=NULL; i=i->next ) {
			Token *token = i->ptr;
			char *t_position = token->position;
			if ( t_position == NULL )
				continue;
			if ( (uintptr_t) t_position < position )
				continue;
			if ( (uintptr_t) t_position > position )
				break;
			do {
				addItem( &s->sequence, token->values );
				if (( i = i->next )) {
					token = i->ptr;
					t_position = token->position;
				}
			} while (( i ) && ((uintptr_t) t_position == position ));
			break;
		}
	}
}
static void
s_register_pop( Parser *p, SchemaThread *s )
{
	if ( p->options & P_TOK ) {
		char *r_id = s->r->rule->identifier;
#ifdef REGEX
		if (( *r_id != '/' ) && r_inbuilt( r_id )) {
#else
		if ( r_inbuilt( r_id ) ) {
#endif
			if (( s->sequence )) {
				reorderListItem( &s->sequence );

				CNString *string = newString();
				for ( listItem *i=s->sequence; i!=NULL; i=i->next )
					StringAppend( string, (int) i->ptr );
				char *str = StringFinish( string, 0 );
				StringReset( string, CNStringMode );
				freeString( string );

				freeListItem( &s->sequence );
				listItem *token = newItem( str );
				addItem( &p->tokens, token );
				addItem( &s->sequence, token );
			}
		}
	}
}
#ifdef REGEX
static void s_regex_feed( Parser *, SchemaThread *, SchemaThread * );
#endif
static void
s_feed( Parser *p, SchemaThread *s, SchemaThread *subscriber )
{
	if ( p->options & P_TOK ) {
#ifdef REGEX
		char *r_id = s->r->rule->identifier;
		if ( *r_id == '/' ) {
			s_regex_feed( p, s, subscriber );
		} else
#endif
		if (( s->sequence )) {
			Sequence *sequence = seqdup( p, s->sequence );
			subscriber->sequence = catListItem( sequence, subscriber->sequence );
		}
	}
	else if (( s->sequence )) {
		// copy s->sequence into a list, reversing order
		listItem *list = NULL;
		for ( listItem *i = s->sequence; i!=NULL; i=i->next ) {
			Pair *pair = i->ptr;
			pair = ( pair->name == NULL ) ?
				newPair( NULL, pair->value ) :
				newPair( pair->name, seqdup( p, pair->value ) );
			addItem( &list, pair );
		}
		// add the list to the subscriber's sequence
		Pair *feed = newPair( s->r->rule->identifier, list );
		addItem( &subscriber->sequence, feed );
	}
}

#ifdef REGEX
static void
s_regex_feed( Parser *p, SchemaThread *s, SchemaThread *subscriber )
{
	/* assumption: subscriber is pending on the '%' of "%/.../"
	   whereas the regex token position was set to ------^
	   so that it cannot be reached by s_advance, aka. s_next
	*/
	Schema *schema = subscriber->schema;
	uintptr_t position = (uintptr_t) subscriber->position + 1;
	for ( listItem *i=schema->data; i!=NULL; i=i->next ) {
		Token *token = i->ptr;
		char *t_position = token->position;
		if ( t_position == NULL )
			continue;
		if ( (uintptr_t) t_position < position )
			continue;
		if ( (uintptr_t) t_position > position )
			break;

		// handle case of multiple token value lists
		listItem *tokens = NULL;
		do {
			addItem( &tokens, token->values );
			if (( i = i->next )) {
				token = i->ptr;
				t_position = token->position;
			}
		}
		while (( i ) && ( (uintptr_t) t_position == position ));

		reorderListItem( &tokens );

		// make sure we do have regex references (optimisation)
		int regref = 0;
		for ( listItem *l=tokens; l!=NULL && !regref; l=l->next ) {
			for ( listItem *j=l->ptr; j!=NULL; j=j->next ) {
				for ( char *str=j->ptr; *str; str++ ) {
					if ( *str != '\\' ) continue;
					else if ( isdigit( str[ 1 ] ) && ( str[ 1 ] != '0' ) )
						{ regref = 1; break; }
					else if ( str[ 1 ] ) str++;
				}
			}
		}
		// expand regex references if there are
		if ( regref ) {
			Regex *r = s->schema->data;
			for ( listItem *l=tokens; l!=NULL; l=l->next ) {
				listItem *values = RegexExpand( r, l->ptr, s->sequence );
				if (( values )) {
					addItem( &p->tokens, values );
					addItem( &subscriber->sequence, values );
				}
			}
		}
		else {
			for ( listItem *j=tokens; j!=NULL; j=j->next )
				addItem( &subscriber->sequence, j->ptr );
		}
		freeListItem( &tokens );

		break;
	}
}
#endif

