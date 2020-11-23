#ifndef REGEX_UTIL_H
#define REGEX_UTIL_H

#include "regex.h"

enum {
	MOVE_ON,
	REPEAT,
	FORK_ON
};

typedef struct {
	char *position;
	listItem *schemata;
} RegexGroup;
typedef char RegexSchema;

typedef struct {
	RegexGroup *group;
	listItem *s;	// { SchemaThread * }
	listItem *f;
	void *subscriber;
} GroupThread;

typedef listItem Sequence;
typedef struct {
	RegexSchema *schema;
	GroupThread *g;
	char *position;
	int icount;
	void *provider;
	Sequence *sequence;
} SchemaThread;

static GroupThread *g_new( Regex *, RegexGroup *, SchemaThread *subscriber );
static void g_free( Regex *, GroupThread * );
static GroupThread *g_launch( Regex *, RegexGroup *, SchemaThread *subscriber, listItem ** );

static SchemaThread *s_new( GroupThread *, RegexSchema * );
static SchemaThread *s_fork( SchemaThread * );
static void s_free( SchemaThread * );
static void s_register( Regex *, SchemaThread *, int event );
static void s_feed( SchemaThread *, SchemaThread * );
static void s_collect( SchemaThread *, SchemaThread * );

static int s_null( RegexSchema * );
static void s_init( char **, RegexSchema * );
static void s_advance( char **, RegexSchema * );

static char *post_p( char * );
static char *post_s( char * );
static int is_suffix( char * );
static RegexGroup *find_group( Regex *, char * );
static int p_test( char *, int event );
static int suffix_test( char *, int );


#endif	// REGEX_UTIL_H
