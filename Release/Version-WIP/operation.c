#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

// #define DEBUG
// #define TRACE
#include "debug.h"
#undef DEBUG
#undef TRACE

#include "string_util.h"
#include "operation.h"
#include "deternarize.h"
#include "deparameterize.h"
#include "expression.h"
#include "instantiate.h"
#include "eeno_query.h"
#include "scour.h"
#include "errout.h"

static int in_condition( int, char *, BMContext *, BMMark ** );
static int in_case( int, char *, BMContext *, BMMark ** );
static int on_event( char *, BMContext *, BMMark ** );
static int on_event_x( int, char *, BMContext *, BMMark ** );
static int do_action( char *, BMContext *, CNStory * );
static int do_enable( char *, BMContext *, listItem *, CNStory *, Registry * );
static int do_enable_x( char *, BMContext *, listItem *, CNStory *, Registry * );
static int do_input( char *, BMContext * );
static int do_output( char *, BMContext * );
static int set_locale( char *, BMContext * );

//===========================================================================
//	bm_operate
//===========================================================================
#define OSUB( i ) (((CNOccurrence *)i->ptr)->sub)

void
bm_operate( CNNarrative *narrative, BMContext *ctx, CNStory *story,
	listItem *narratives, Registry *subs ) {
	DBG_OPERATE_BGN
	listItem *i = newItem( narrative->root );
	listItem *stack = NULL;
	BMMark *marked = NULL;
	int passed = 1;
	for ( ; ; ) {
		CNOccurrence *occurrence = i->ptr;
		int type = cast_i( occurrence->data->type );
		if (!( type&ELSE && passed )) {
			type &= ~ELSE; // Assumption: ELSE handled as ROOT#=0
			char *expression = occurrence->data->expression;
			char *deternarized = bm_deternarize( &expression, type, ctx );
			listItem *j = occurrence->sub;
			BMMark **mark = (( j ) ? &marked : NULL );
			if ( type&(SWITCH|CASE) ) {
				passed = in_case( type, expression, ctx, mark ); }
			else switch ( type ) {
			case ROOT:   passed = 1; break;
			case IN:     passed = in_condition( 0, expression, ctx, mark ); break;
			case IN_X:   passed = in_condition( AS_PER, expression, ctx, mark ); break;
			case ON:     passed = on_event( expression, ctx, mark ); break;
			case ON_X:   passed = on_event_x( 0, expression, ctx, mark ); break;
			case PER_X:  passed = on_event_x( AS_PER, expression, ctx, mark ); break;
			case DO:     do_action( expression, ctx, story ); break;
			case EN:     do_enable( expression, ctx, narratives, story, subs ); break;
			case EN_X:   do_enable_x( expression, ctx, narratives, story, subs ); break;
			case INPUT:  do_input( expression, ctx ); break;
			case OUTPUT: do_output( expression, ctx ); break;
			case LOCALE: set_locale( expression, ctx ); break; }
			if (( deternarized )) free( expression );
			// pushing down
			if ( passed && (( j ) || type&CASE )) {
				while ( !j ) { i=i->next; j=OSUB( i ); }
				bm_context_mark( ctx, marked );
				addItem( &stack, i );
				addItem( &stack, marked );
				i = j; marked = NULL;
				continue; } }
		// popping up
		for ( ; ; ) {
			marked = bm_context_unmark( ctx, marked );
			if (( marked )) { // "per" result
				occurrence = i->ptr;
				bm_context_mark( ctx, marked );
				addItem( &stack, i );
				addItem( &stack, marked );
				i = occurrence->sub;
				marked = NULL; break; }
			else if (( i->next )) {
				i = i->next; break; }
			else if (( stack )) {
				passed = 1; // otherwise we would not be here
				marked = popListItem( &stack );
				i = popListItem( &stack ); }
			else goto RETURN; } }
RETURN:
	freeItem( i );
	DBG_OPERATE_END }

//---------------------------------------------------------------------------
//	in_condition, in_case
//---------------------------------------------------------------------------
static int
in_condition( int as_per, char *expression, BMContext *ctx, BMMark **mark )
/*
	Assumption: if (( mark )) then *mark==NULL to begin with
*/ {
	DBG_IN_CONDITION_BGN
	int success=0, negated=0;
	if ( !strncmp( expression, "~.:", 3 ) )
		{ negated=1; expression+=3; }

	void *found;
	if ( !strncmp( expression, "?:\"", 3 ) ) {
		as_per = 0; // no need
		CNDB *db = BMContextDB( ctx );
		found = db_arena_lookup( expression+2, db );
		if (( found )) success = 1; }
	else if ( strcmp( expression, "~." ) ) {
		if ( !strncmp( expression, "(~.:", 4 ) ) {
			expression += 4; negated = !negated; }
		if ( as_per ) found = bm_scan( expression, ctx );
		else found = bm_feel( BM_CONDITION, expression, ctx );
		if (( found )) success = 1; }

	if ( negated ) success = !success;
	else if ( success && ( mark ))
		*mark = bm_mark( expression, NULL, as_per, found );
	DBG_IN_CONDITION_END
	return success; }

static int
in_case( int type, char *expression, BMContext *ctx, BMMark **mark )
/*
	evaluate IN ":expression:" and inform lmark:[ ^^, *^^ ] if success
	Note: *^^ can be NULL
*/ {
	DBG_IN_CASE_BGN
	int success = 0;
	if ( type&SWITCH ) {
		type = ( type&IN ? BM_CONDITION : BM_INSTANTIATED );
		Pair *found = bm_switch( type, expression, ctx );
		if (( found )) {
			if (( mark )) *mark = bm_lmark( found );
			else freePair( found );
			success = 1; } }
	else success = bm_case( expression, ctx );
	DBG_IN_CASE_END
	return success; }

//---------------------------------------------------------------------------
//	on_event, on_case, case_on
//---------------------------------------------------------------------------
static int
on_event( char *expression, BMContext *ctx, BMMark **mark )
/*
	Assumption: if (( mark )) then *mark==NULL to begin with
*/ {
	DBG_ON_EVENT_BGN
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
	else if ( success==2 && ( mark ))
		*mark = bm_mark( expression, NULL, 0, found );
	DBG_ON_EVENT_END
	return success; }

static int
on_case( char *expression, BMContext *ctx, BMMark **mark )
/*
	evaluate ON ":expression:" and inform lmark:[ ^^, *^^ ] if success
	Note: *^^ can be NULL
*/ {
	return 1; }

static int
case_on( char *expression, BMContext *ctx, BMMark **mark )
/*
	evaluate ON "expression" (including "~.") and push qmark if there is
*/ {
	return 1; }

//---------------------------------------------------------------------------
//	on_event_x
//---------------------------------------------------------------------------
typedef int ProxyTest( CNInstance *proxy );
static inline void * eeno_test( int, ProxyTest *, listItem ** );
static inline void * eeno_feel( int, int, char *, listItem **, BMContext * );
static inline void eeno_release( int, void * );

static int
on_event_x( int as_per, char *expression, BMContext *ctx, BMMark **mark )
/*
	Assumption: if (( mark )) then *mark==NULL to begin with
*/ {
	DBG_ON_EVENT_X_BGN
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
	else if ( success && ( mark ))
		*mark = success==2 ? 
			bm_mark( expression, src, as_per|EENOK, found ) :
			bm_mark( NULL, src, as_per|EENOK, found );
	DBG_ON_EVENT_X_END
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

//---------------------------------------------------------------------------
//	do_action
//---------------------------------------------------------------------------
static int
do_action( char *expression, BMContext *ctx, CNStory *story ) {
	DBG_DO_ACTION_BGN
	if ( !strncmp(expression,"~(",2) )
		bm_release( expression+1, ctx );
	else if ( !strcmp(expression, ":." ) ) // reenter
		bm_instantiate( ":%(:?)", ctx, story );
	else if ( !strcmp( expression, "exit" ) )
		db_exit( BMContextDB(ctx) );
	else if ( strncmp( expression, "~.", 2 ) )
		bm_instantiate( expression, ctx, story );
	DBG_DO_ACTION_END
	return 1; }

//---------------------------------------------------------------------------
//	do_enable
//---------------------------------------------------------------------------
static inline void query( char *, BMContext *, CNNarrative *, void *, Registry * );
typedef struct {
	CNNarrative *narrative;
	void *key;
	Registry *subs;
	Pair *entry;
	} EnableData;

static int
do_enable( char *en, BMContext *ctx, listItem *narratives, CNStory *story, Registry *subs ) {
	if ( subs==NULL ) { errout( EnPostFrame, en ); return 1; }
	DBG_DO_ENABLE
	// post-frame narrative query
	if ( *en=='&' ) {
		for ( listItem *i=narratives->next; i!=NULL; i=i->next ) {
			CNNarrative *narrative = i->ptr;
			char *p = narrative->proto;
			if ( *p=='.' && p[2]=='&' ) {
				registryRegister( subs, narrative, NULL );
				// multiple definitions allowed but not executed
				return 1; } }
		return 0; }
	// en query
	for ( listItem *i=narratives->next; i!=NULL; i=i->next ) {
		CNNarrative *narrative = i->ptr;
		char *p = narrative->proto, *q;
		if ( *p!='.' || p[2]=='&' ) continue;
		p = p_prune( PRUNE_IDENTIFIER, p+1 );
		p++; // skip leading ':'
		CNString *s = NULL;
		switch ( *p ) {
		case '"':
			query( en, ctx, narrative, db_arena_key(p), subs );
			break;
		default:
			// build query string "proto:en"
			s = bm_deparameterize( p );
			s_add( ":" )
			s_add( en )
			q = StringFinish( s, 0 );
#ifdef DEBUG
			fprintf( stderr, "do_enable: built '%s'\n", q );
			if ( !strncmp( q, "%(.,...):", 9 ) )
				errout( OperationProtoPassThrough, p );
#endif
			query( q, ctx, narrative, NULL, subs );
			freeString( s ); } }
	return 1; }

static BMQueryCB enable_CB;
static inline void
query( char *q, BMContext *ctx, CNNarrative *n, void *key, Registry *subs ) {
	EnableData data;
	data.subs = subs;
	data.narrative = n;
	data.key = key;
	data.entry = NULL;
	bm_query( BM_CONDITION, q, ctx, enable_CB, &data ); }

static BMQTake
enable_CB( CNInstance *e, BMContext *ctx, void *user_data ) {
	EnableData *data = user_data;
	if (( data->key )) {
		uint *master = (uint *) &data->key;
		uint *key = CNSharedKey( e );
		if ( cnIsShared(e) && key[0]==master[0] && key[1]==master[1] ) {
			registryRegister( data->subs, data->narrative, newItem(e) );
			return BMQ_DONE; } }
	else {
		Pair *entry = data->entry;
		if ( !entry ) {
			entry = registryRegister( data->subs, data->narrative, NULL );
			data->entry = entry; }
		addIfNotThere((listItem **) &entry->value, e ); }
	return BMQ_CONTINUE; }

//---------------------------------------------------------------------------
//	do_enable_x
//---------------------------------------------------------------------------
static int
do_enable_x( char *en, BMContext *ctx, listItem *narratives, CNStory *story, Registry *subs ) {
	if ( subs==NULL ) { errout( EnPostFrame, en ); return 1; }
	DBG_DO_ENABLE_X
	for ( listItem *i=narratives->next; i!=NULL; i=i->next ) {
		CNNarrative *narrative = i->ptr;
		char *p = narrative->proto, *q;
		if ( *p++!=':' ) continue;
		// we have p: "func(params)" and en: "func expr"
		if ( !strcomp( p, en, 1 ) ) {
			p = p_prune( PRUNE_IDENTIFIER, p );
			q = p_prune( PRUNE_IDENTIFIER, en );
			// build query string "(params):expr"
			CNString *s = bm_deparameterize( p );
			s_add( ":" )
			s_add( q )
			q = StringFinish( s, 0 );
			query( q, ctx, narrative, NULL, subs );
			freeString( s );
			// multiple definitions allowed but not executed
			return 1; } }
	return 0; }

//---------------------------------------------------------------------------
//	do_input
//---------------------------------------------------------------------------
static int
do_input( char *expression, BMContext *ctx )
/*
	Assuming expression is in one of the following forms
		:<
		arg <
		arg, fmt <
	reads input from stdin and assign arg according to fmt
*/ {
	DBG_DO_INPUT_BGN
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
	DBG_DO_INPUT_END
	return retval; }

//---------------------------------------------------------------------------
//	do_output
//---------------------------------------------------------------------------
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
	DBG_DO_OUTPUT_BGN
	int retval;
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
	retval = bm_outputf( stream, fmt, args, ctx );

	// cleanup
	freeListItem( &args );
	DBG_DO_OUTPUT_END
	return retval; }

//---------------------------------------------------------------------------
//	set_locale
//---------------------------------------------------------------------------
static int
set_locale( char *expression, BMContext *ctx ) {
	DBG_SET_LOCALE
#ifdef DBG_TRACE_LOCALE
	printf( "( %s, \"%s\", ctx )\n", "LOCALE", expression );
#endif
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

//===========================================================================
//	bm_op, bm_vop	- useful for debugging
//===========================================================================
void
bm_op( int type, char *expression, BMContext *ctx, CNStory *story ) {
	switch ( type ) {
	case DO: do_action( expression, ctx, story ); break;
	case INPUT: do_input( expression, ctx ); break;
	case OUTPUT: do_output( expression, ctx ); break;
	default: errout( OperationNotSupported ); } }

int
bm_vop( int type, ... ) {
	va_list ap;
	va_start( ap, type );
	int passed = 1;
	if ( type&(SWITCH|CASE) ) {
		passed = in_case(
			va_arg( ap, int ),		// type
			va_arg( ap, char * ),		// expression
			va_arg( ap, BMContext *),	// context
			va_arg( ap, BMMark ** ) ); }	// mark
	else switch ( type ) {
	case IN: passed = in_condition(
		0,				// as_per
		va_arg( ap, char * ),		// expression
		va_arg( ap, BMContext * ),	// ctx
		va_arg( ap, BMMark ** ) );	// mark
		break;
	case IN_X: passed = in_condition(
		AS_PER,				// as_per
		va_arg( ap, char * ),		// expression
		va_arg( ap, BMContext * ),	// ctx
		va_arg( ap, BMMark ** ) );	// mark
		break;
	case ON: passed = on_event(
		va_arg( ap, char * ),		// expression
		va_arg( ap, BMContext * ),	// ctx
		va_arg( ap, BMMark ** ) );	// mark
		break;
	case ON_X: passed = on_event_x(
		0,				// as_per
		va_arg( ap, char * ),		// expression
		va_arg( ap, BMContext * ),	// ctx
		va_arg( ap, BMMark ** ) );	// mark
		break;
	case PER_X: passed = on_event_x(
		AS_PER,				// as_per
		va_arg( ap, char * ),		// expression
		va_arg( ap, BMContext * ),	// ctx
		va_arg( ap, BMMark ** ) );	// mark
		break;
	case DO: do_action(
		va_arg( ap, char * ),		// expression
		va_arg( ap, BMContext * ),	// ctx
		va_arg( ap, CNStory * ) );	// story
		break;
	case INPUT: do_input(
		va_arg( ap, char * ),		// expression
		va_arg( ap, BMContext * ) );	// ctx
		break;
	case OUTPUT: do_output(
		va_arg( ap, char * ),		// expression
		va_arg( ap, BMContext * ) );	// ctx
		break;
	case EN: do_enable(
		va_arg( ap, char * ),		// expression
		va_arg( ap, BMContext * ),	// ctx
		va_arg( ap, listItem * ),	// narratives
		va_arg( ap, CNStory * ),	// story
		va_arg( ap, Registry * ) );	// subs
		break;
	case EN_X: do_enable_x(
		va_arg( ap, char * ),		// expression
		va_arg( ap, BMContext * ),	// ctx
		va_arg( ap, listItem * ),	// narratives
		va_arg( ap, CNStory * ),	// story
		va_arg( ap, Registry * ) );	// subs
		break;
	case LOCALE: set_locale(
		va_arg( ap, char * ),		// expression
		va_arg( ap, BMContext * ) );	// ctx
		break; }
	va_end( ap );
	return passed; }

