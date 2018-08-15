#ifndef PARSER_UTIL_H
#define PARSER_UTIL_H

#include "parser.h"
#include "scheme.h"
#include "stream.h"

#define ParserSpace( event ) \
	is_space( event )
#define ParserSeparator( event ) \
	is_separator( event )
 
/* A RuleThread r has
	. subscribers: r->subscribers
	. schema-threads: r->s
   Subscribers are dependent on the rule completion to progress
   Rules are dependent on schema-threads completion to complete
*/
typedef struct {
	Rule *rule;
	listItem *s;	// { SchemaThread * }
	listItem *f;
	listItem *subscribers;
} RuleThread;

typedef listItem Sequence;
typedef struct {
	Schema *schema;
	RuleThread *r;
	char *position;
	int consumed;
	Sequence *sequence;
} SchemaThread;

static RuleThread *r_new( Parser *, Rule *, SchemaThread *subscriber );
static void r_free( Parser *, RuleThread * );
static int r_inbuilt( char * );
static Rule *fetch_inbuilt( char * );
static RuleThread *r_launch_space( Parser *, SchemaThread *subscriber, listItem ** );
static RuleThread *r_launch_inbuilt( Parser *, SchemaThread *subscriber, listItem ** );
static RuleThread *r_launch( Parser *, Rule *, SchemaThread *subscriber, listItem ** );

static SchemaThread *s_new( RuleThread *, Schema * );
static SchemaThread *s_fork( Parser *, SchemaThread * );
static void s_free( Parser *, SchemaThread * );
static void s_register_push( Parser *, SchemaThread * );
static void s_register( Parser *, SchemaThread *, int event );
static void s_register_advance( Parser *, SchemaThread * );
static void s_register_pop( Parser *, SchemaThread * );
static void s_feed( Parser *, SchemaThread *, SchemaThread * );

static int s_null( Schema * );
static void s_init( char **, Schema * );
static void s_advance( char **, Schema * );


#endif	// PARSER_UTIL_H
