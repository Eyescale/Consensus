#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "operation.h"
#include "deternarize.h"
#include "deparameterize.h"
#include "expression.h"
#include "instantiate.h"
#include "eeno_query.h"
#include "scour.h"

// #define DEBUG

static int in_condition( int, char *, BMContext *, MarkData **mark );
static int on_event( char *, BMContext *, MarkData **mark );
static int on_event_x( int, char *, BMContext *, MarkData **mark );
static int do_action( char *, BMContext *, CNStory *story );
static int do_enable( char *, BMContext *, listItem *, CNStory *, Registry * );
static int do_input( char *, BMContext * );
static int do_output( char *, BMContext * );
static int set_locale( char *, BMContext * );

//===========================================================================
//	bm_op
//===========================================================================
void
bm_op( int type, char *expression, BMContext *ctx, CNStory *story ) {
	switch ( type ) {
	case DO: do_action( expression, ctx, story ); break;
	case INPUT: do_input( expression, ctx ); break;
	case OUTPUT: do_output( expression, ctx ); break;
	default: fprintf( stderr, "B%%: Warning: operation not supported\n" ); } }

//===========================================================================
//	bm_operate
//===========================================================================
void
bm_operate( CNNarrative *narrative, BMContext *ctx, CNStory *story,
	listItem *narratives, Registry *subs ) {
#ifdef DEBUG
	fprintf( stderr, "operate bgn\n" );
#endif
	listItem *i = newItem( narrative->root );
	listItem *stack = NULL;
	MarkData *marked = NULL;
	int passed = 1;
	for ( ; ; ) {
		CNOccurrence *occurrence = i->ptr;
		int type = cast_i( occurrence->data->type );
		if (!( type&ELSE && passed )) {
			// processing
			listItem *j = occurrence->sub;
			if ( !marked ) {
				MarkData **mark = (( j ) ? &marked : NULL );
				char *expression, *deternarized = NULL;
				if (( type&=~ELSE )) { // Assumption: ELSE handled as ROOT#=0
					expression = occurrence->data->expression;
					deternarized = bm_deternarize( &expression, type, ctx ); }
				switch ( type ) {
				case ROOT: passed=1; break;
				case IN: passed=in_condition( 0, expression, ctx, mark ); break;
				case IN_X: passed=in_condition( AS_PER, expression, ctx, mark ); break;
				case ON: passed=on_event( expression, ctx, mark ); break;
				case ON_X: passed=on_event_x( 0, expression, ctx, mark ); break;
				case PER_X: passed=on_event_x( AS_PER, expression, ctx, mark ); break;
				case DO: do_action( expression, ctx, story ); break;
				case INPUT: do_input( expression, ctx ); break;
				case OUTPUT: do_output( expression, ctx ); break;
				case EN: do_enable( expression, ctx, narratives, story, subs ); break;
				case LOCALE: set_locale( expression, ctx ); break; }
				if (( deternarized )) free( expression ); }
			// pushing down
			if ( passed && ( j )) {
				bm_context_mark( ctx, marked );
				addItem( &stack, i );
				addItem( &stack, marked );
				i = j; marked = NULL;
				continue; } }
		// popping up
		for ( ; ; ) {
			marked = bm_context_unmark( ctx, marked );
			if (( marked )) // "per" result
				break;
			else if (( i->next )) {
				i = i->next;
				break; }
			else if (( stack )) {
				passed = 1; // otherwise we would not be here
				marked = popListItem( &stack );
				i = popListItem( &stack ); }
			else goto RETURN; } }
RETURN:
	freeItem( i );
#ifdef DEBUG
	fprintf( stderr, "operate end\n" );
#endif
	}

//===========================================================================
//	in_condition
//===========================================================================
static int
in_condition( int as_per, char *expression, BMContext *ctx, MarkData **mark )
/*
	Assumption: if (( mark )) then *mark==NULL to begin with
*/ {
#ifdef DEBUG
	fprintf( stderr, "in condition bgn: %s\n", expression );
#endif
	Pair *m;
	int success=0, negated=0;
	if ( !strncmp( expression, "~.:", 3 ) )
		{ negated=1; expression+=3; }

	void *found;
	if ( strcmp( expression, "~." ) ) {
		if ( as_per ) found = bm_scan( expression, ctx );
		else found = bm_feel( BM_CONDITION, expression, ctx );
		if (( found )) success = 1; }

	if ( negated ) success = !success;
	else if ( success && ( mark )) {
		if (( m=bm_mark( expression, NULL ) )) {
			m->name = cast_ptr( as_per|cast_i(m->name) );
			*mark = (MarkData *) newPair( m, found ); } }
#ifdef DEBUG
	fprintf( stderr, "in_condition end\n" );
#endif
	return success; }

//===========================================================================
//	on_event
//===========================================================================
static int
on_event( char *expression, BMContext *ctx, MarkData **mark )
/*
	Assumption: if (( mark )) then *mark==NULL to begin with
*/ {
#ifdef DEBUG
	fprintf( stderr, "on_event bgn: %s\n", expression );
#endif
	Pair *m;
	int success=0, negated=0;
	if ( !strncmp(expression,"~.:",3) )
		{ negated=1; expression+=3; }

	CNInstance *found;
	if ( !strcmp( expression, "init" ) ) {
		CNDB *db = BMContextDB( ctx );
		success = DBInitOn( db ); }
	else if ( !strcmp( expression, "." ) ) {
		CNDB *db = BMContextDB( ctx );
		success = DBActive( db ); }
	else if ( !strncmp(expression,"~(",2) ) {
		expression++;
		found = bm_feel( BM_RELEASED, expression, ctx );
		if (( found )) success = 2; }
	else {
		found = bm_feel( BM_INSTANTIATED, expression, ctx );
		if (( found )) success = 2; }

	if ( negated ) success = !success;
	else if ( success==2 && ( mark )) {
		if (( m=bm_mark( expression, NULL ) )) {
			*mark = (MarkData *) newPair( m, found ); } }
#ifdef DEBUG
	fprintf( stderr, "on_event end\n" );
#endif
	return success; }

//===========================================================================
//	on_event_x
//===========================================================================
typedef int ProxyTest( CNInstance *proxy );
static inline void * eeno_test( int, ProxyTest *, listItem ** );
static inline void * eeno_feel( int, int, char *, listItem **, BMContext * );
static inline void eeno_release( int, void * );

static int
on_event_x( int as_per, char *expression, BMContext *ctx, MarkData **mark )
/*
	Assumption: if (( mark )) then *mark==NULL to begin with
*/ {
#ifdef DEBUG
	if ( as_per ) fprintf( stderr, "on_event_x bgn: per %s\n", expression );
	else fprintf( stderr, "on_event_x bgn: on %s\n", expression );
#endif
	Pair *m;
	int success=0, negated=0;
	if ( !strncmp(expression,"~.:",3) )
		{ negated=1; expression+=3; }

	char *src = p_prune( PRUNE_TERM, expression ) + 1;
	if ( *expression==':' ) src = p_prune( PRUNE_TERM, src ) + 1;
	listItem *proxies = bm_eeno_scan( src, ctx );
	if ( !proxies ) return 0;

	Pair *found = NULL;
	if ( !strncmp( expression, "init<", 5 ) ) {
		found = eeno_test( as_per, bm_proxy_in, &proxies );
		if (( found )) success = 1; }
	else if ( !strncmp( expression, "exit<", 5 ) ) {
		found = eeno_test( as_per, bm_proxy_out, &proxies );
		if (( found )) success = 1; }
	else if ( !strncmp(expression,".<",2) ) {
		found = eeno_test( as_per, bm_proxy_active, &proxies );
		if (( found )) success = 1; }
	else if ( !strncmp(expression,"~(",2) ) {
		expression++;
		found = eeno_feel( as_per, BM_RELEASED, expression, &proxies, ctx );
		if (( found )) success = 2; }
	else {
		found = eeno_feel( as_per, BM_INSTANTIATED, expression, &proxies, ctx );
		if (( found )) success = 2; }

	if ( negated ) {
		success = !success;
		eeno_release( as_per, found ); }
	else if ( success && ( mark )) {
		switch ( success ) {
		case 1: m = bm_mark( NULL, src ); break;
		case 2: m = bm_mark( expression, src ); }
		if (( m )) {
			m->name = cast_ptr( as_per|cast_i(m->name)|EENOK );
			*mark = (MarkData *) newPair( m, found ); } }
#ifdef DEBUG
	fprintf( stderr, "on_event_x end\n" );
#endif
	return success; }

static inline void *
eeno_test( int as_per, ProxyTest *func, listItem **proxies ) {
	CNInstance *proxy;
	if ( as_per ) {
		listItem *results = NULL;
		while (( proxy = popListItem(proxies) )) {
			if ( func( proxy ) ) {
				addItem( &results, newPair( NULL, proxy ) ); } }
		return results; }
	else {
		while (( proxy = popListItem(proxies) )) {
			if ( func( proxy ) ) {
				freeListItem( proxies );
				return newPair( NULL, proxy ); } }
		return NULL; } }

static inline void *
eeno_feel( int as_per, int type, char *p, listItem **proxies, BMContext *ctx ) {
	CNInstance *proxy;
	void *found;
	if ( as_per ) {
		listItem *results = NULL;
		while (( proxy = popListItem(proxies) )) {
			found = bm_eeno_query( type|BM_AS_PER, p, proxy, ctx );
			if (( found )) {
				Pair *batch = newPair( found, proxy );
				addItem( &results, batch ); } }
		return results; }
	else {
		while (( proxy = popListItem(proxies) )) {
			found = bm_eeno_query( type, p, proxy, ctx );
			if (( found )) {
				freeListItem( proxies );
				return newPair( found, proxy ); } }
		return NULL; } }

static inline void
eeno_release( int as_per, void *found ) {
	Pair *batch;
	if (( found )) {
		if ( as_per ) {
			while (( batch=popListItem((listItem **)&found) )) {
				freeListItem((listItem **) &batch->name );
				freePair( batch ); } }
		else freePair( found ); } }

//===========================================================================
//	do_action
//===========================================================================
static int
do_action( char *expression, BMContext *ctx, CNStory *story ) {
#ifdef DEBUG
	fprintf( stderr, "do_action: %s\n", expression );
#endif
	if ( !strncmp(expression,"~(",2) )
		bm_release( expression+1, ctx );
	else if ( !strcmp( expression, "exit" ) )
		db_exit( BMContextDB(ctx) );
	else if ( strcmp( expression, "~." ) )
		bm_instantiate( expression, ctx, story );
#ifdef DEBUG
	fprintf( stderr, "do_action end\n" );
#endif
	return 1; }

//===========================================================================
//	do_input
//===========================================================================
static int
do_input( char *expression, BMContext *ctx )
/*
	Assuming expression is in one of the following forms
		:<
		arg <
		arg, fmt <
	reads input from stdin and assign arg according to fmt
*/ {
#ifdef DEBUG
	fprintf( stderr, "do_input bgn: %s\n", expression );
#endif
	CNDB *db = BMContextDB( ctx );
	if ( DBExitOn(db) ) return 0;

	// extract fmt and args:< expression(s) >
	listItem *args = NULL;
	char *p = expression, *fmt;
	switch ( *p ) {
	case ':':
		return ( p[1]=='<' ? getchar() : -1 );
	case '<':
		// VECTOR - not yet supported
		return -1;
	default: ;
		char *q = p_prune( PRUNE_TERM, p ) + 1;
		args = newItem( p );
		fmt = ( *q=='"' ? q : "" ); }

	// invoke bm_inputf()
	int retval = bm_inputf( fmt, args, ctx ) ;

	// cleanup
	freeListItem( &args );
#ifdef DEBUG
	fprintf( stderr, "do_input end\n" );
#endif
	return retval; }

//===========================================================================
//	do_output
//===========================================================================
static int
do_output( char *expression, BMContext *ctx )
/*
	Assuming expression is in one the following forms
		> fmt
		> fmt : expression
		> fmt : < expression1, expression2, ... >
		>:
		>: expression
		>: < expression1, expression2, ... >
	resp. >& instead of >

	then outputs expression(s) to stdout resp. stderr
	according to fmt
*/ {
#ifdef DEBUG
	fprintf( stderr, "do_output bgn: %s\n", expression );
#endif
	if ( !strcmp( expression, ">:." ) )
		expression = ">:~%(?,.):~%(.,?)";

	// extracts fmt and args:{ expression(s) }
	char *p = expression + 1; // skipping the leading '>'
	FILE *stream;
	if ( *p=='&' ) { stream = stderr; p++; }
	else stream = stdout;
	listItem *args = NULL;
	char *fmt = ( *p=='"' ? p : "" );
	if ( *fmt ) p = p_prune( PRUNE_FILTER, p );
	if ( *p==':' ) {
		p++;
		if ( *p=='<' ) {
			do {	p++;
				addItem( &args, p );
				p = p_prune( PRUNE_TERM, p );
			} while ( *p!='>' );
			reorderListItem( &args ); }
		else if ( *p )
			addItem( &args, p ); }

	// invoke bm_outputf()
	int retval = bm_outputf( stream, fmt, args, ctx );

	// cleanup
	freeListItem( &args );
#ifdef DEBUG
	fprintf( stderr, "do_output end\n" );
#endif
	return retval; }

//===========================================================================
//	do_enable
//===========================================================================
static BMQueryCB enable_CB;
typedef struct {
	CNNarrative *narrative;
	void *string_ref;
	Registry *subs;
	Pair *entry;
	} EnableData;

static int
do_enable( char *en, BMContext *ctx, listItem *narratives, CNStory *story, Registry *subs ) {
	Registry *string_arena = story->arena->name;
	CNDB *db = BMContextDB( ctx );
	EnableData data;
	data.subs = subs;
	for ( listItem *i=narratives->next; i!=NULL; i=i->next ) {
		CNNarrative *narrative = i->ptr;
		char *p = narrative->proto;
		switch ( *p ) {
		case '.': p = p_prune(PRUNE_IDENTIFIER,p+1) + 1; }
		void *string_ref = NULL;
		CNString *s = NULL;
		char *query;
		switch ( *p ) {
		case '"':
			string_ref = narrative->root->data->expression;
			if ( !string_ref ) { // cache arena string's [ s, ref ]
				string_ref = registryLookup( string_arena, p );
				narrative->root->data->expression = string_ref; }
			query = en;
			break;
		default:
			// build query string "proto:en"
			s = bm_deparameterize( p );
			StringAppend( s, ':' );
			s_add( en )
			query = StringFinish( s, 0 ); }
#ifdef DEBUG
		if ( !strncmp( query, "%(.,...):", 9 ) ) {
			fprintf( stderr, ">>>>> B%%: do_enable(): Warning: "
				"proto %s resolves to %%(.,...) - "
				"passes through!!\n", p ); }
#endif
		// launch enable query
		data.narrative = narrative;
		data.string_ref = string_ref;
		data.entry = NULL;
		bm_query( BM_CONDITION, query, ctx, enable_CB, &data );
		if (( s )) freeString( s ); }
	return 1; }

static BMQTake
enable_CB( CNInstance *e, BMContext *ctx, void *user_data ) {
	EnableData *data = user_data;
	void *string_ref = data->string_ref;
	if ( !string_ref ) {
		Pair *entry = data->entry;
		if ( !entry ) {
			entry = registryRegister( data->subs, data->narrative, NULL );
			data->entry = entry; }
		addIfNotThere((listItem **) &entry->value, e ); }
	else if ( e->sub[1]==string_ref ) {
		registryRegister( data->subs, data->narrative, newItem(e) );
		return BMQ_DONE; }
	return BMQ_CONTINUE; }

//===========================================================================
//	set_locale
//===========================================================================
static int
set_locale( char *expression, BMContext *ctx ) {
	listItem **list;
	if ( !strncmp(expression,".%",2) ) {
		expression+=2; // skip '.%'
		char *p = p_prune( PRUNE_IDENTIFIER, expression );
		switch ( *p ) {
		case '\0':
		case ' ': return bm_tag_register( expression, p, ctx );
		case '<': return bm_tag_inform( expression, p, ctx );
		case '{': return bm_tag_traverse( expression, p, ctx );
		case '~':
			switch ( p[1] ) {
			case '{': return bm_tag_traverse( expression, p, ctx );
			case '\0': return bm_tag_clear( expression, ctx ); } }
		return 0; }
	else {
		return bm_register_locale( ctx, expression ); } }
