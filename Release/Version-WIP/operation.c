#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "cell.h"
#include "operation.h"
#include "deternarize.h"
#include "deparameterize.h"
#include "expression.h"
#include "instantiate.h"
#include "eeno_feel.h"

// #define DEBUG

static int in_condition( char *, BMContext *, Pair **mark );
static int on_event( char *, BMContext *, Pair **mark );
static int on_event_x( char *, int pre, BMContext *, Pair **mark );
static int do_action( char *, BMContext *, CNStory *story );
static int do_enable( Registry *, listItem *, char *, BMContext * );
static int do_input( char *, BMContext * );
static int do_output( char *, BMContext * );

//===========================================================================
//	bm_operate
//===========================================================================
void
bm_operate( CNNarrative *narrative, CNInstance *instance, BMContext *ctx,
	Registry *subs, listItem *narratives, CNStory *story )
{
#ifdef DEBUG
	fprintf( stderr, "operate bgn\n" );
#endif
	bm_context_actualize( ctx, narrative->proto, instance );
	listItem *i = newItem( narrative->root );
	listItem *stack = NULL;
	Pair *marked = NULL;
	int passed = 1;
	for ( ; ; ) {
		CNOccurrence *occurrence = i->ptr;
		int type = occurrence->data->type;
		if (!( type&ELSE && passed )) {
			// processing
			listItem *j = occurrence->sub;
			if ( !marked ) {
				type &= ~ELSE; // Assumption: ELSE will be handled as ROOT#=0
				char *expression = occurrence->data->expression;
				char *deternarized = bm_deternarize( &expression, type, ctx );
				Pair **mark = (( j ) ? &marked : NULL );
				switch ( type ) {
				case ROOT: passed=1; break;
				case IN: passed=in_condition( expression, ctx, mark ); break;
				case ON: passed=on_event( expression, ctx, mark ); break;
				case ON_X: passed=on_event_x( expression, 0, ctx, mark ); break;
				case PER_X: passed=on_event_x( expression, BM_AS_PER, ctx, mark ); break;
				case DO: do_action( expression, ctx, story ); break;
				case INPUT: do_input( expression, ctx ); break;
				case OUTPUT: do_output( expression, ctx ); break;
				case EN: do_enable( subs, narratives, expression, ctx ); break;
				case LOCALE: bm_register_locale( ctx, expression ); break; }
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
	bm_context_release( ctx );
#ifdef DEBUG
	fprintf( stderr, "operate end\n" );
#endif
}

//===========================================================================
//	in_condition
//===========================================================================
static int
in_condition( char *expression, BMContext *ctx, Pair **mark )
/*
	Assumption: if (( mark )) then *mark==NULL to begin with
*/
{
#ifdef DEBUG
	fprintf( stderr, "in condition bgn: %s\n", expression );
#endif
	int success=0, negated=0;
	if ( !strncmp( expression, "~.:", 3 ) )
		{ negated=1; expression+=3; }

	CNInstance *found;
	if ( strcmp( expression, "~." ) ) {
		found = bm_feel( BM_CONDITION, expression, ctx );
		if (( found )) success = 1; }

	if ( negated ) success = !success;
	else if (( mark ) && success )
		*mark = bm_mark( 0, expression, NULL, found );
#ifdef DEBUG
	fprintf( stderr, "in_condition end\n" );
#endif
	return success;
}

//===========================================================================
//	on_event
//===========================================================================
static int
on_event( char *expression, BMContext *ctx, Pair **mark )
/*
	Assumption: if (( mark )) then *mark==NULL to begin with
*/
{
#ifdef DEBUG
	fprintf( stderr, "on_event bgn: %s\n", expression );
#endif
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
	else if (( mark ) && (success==2 ))
		*mark = bm_mark( 0, expression, NULL, found );
#ifdef DEBUG
	fprintf( stderr, "on_event end\n" );
#endif
	return success;
}

//===========================================================================
//	on_event_x
//===========================================================================
typedef int ProxyTest( CNInstance *proxy );
static inline void * _test( int, ProxyTest *, listItem ** );
static inline void * _feel( char *, int, listItem **, BMContext * );
static inline void _release( int, void * );

static int
on_event_x( char *expression, int pre, BMContext *ctx, Pair **mark )
/*
	Assumptions:
		pre is either 0 or BM_AS_PER
		if (( mark )) then *mark==NULL to begin with
*/
{
#ifdef DEBUG
	if ( pre ) fprintf( stderr, "on_event_x bgn: per %s\n", expression );
	else fprintf( stderr, "on_event_x bgn: on %s\n", expression );
#endif
	int success=0, negated=0;
	if ( !strncmp(expression,"~.:",3) )
		{ negated=1; expression+=3; }

	char *src = p_prune( PRUNE_TERM, expression ) + 1;
	if ( *expression==':' ) src = p_prune( PRUNE_TERM, src ) + 1;
	listItem *proxies = bm_proxy_scan( BM_CONDITION, src, ctx );
	if ( !proxies ) return 0;

	Pair *found = NULL;
	if ( !strncmp( expression, "init<", 5 ) ) {
		found = _test( pre, bm_proxy_in, &proxies );
		if (( found )) success = 1; }
	else if ( !strncmp( expression, "exit<", 5 ) ) {
		found = _test( pre, bm_proxy_out, &proxies );
		if (( found )) success = 1; }
	else if ( !strncmp(expression,".<",2) ) {
		found = _test( pre, bm_proxy_active, &proxies );
		if (( found )) success = 1; }
	else if ( !strncmp(expression,"~(",2) ) {
		expression++;
		found = _feel( expression, pre|BM_RELEASED, &proxies, ctx );
		if (( found )) success = 2; }
	else {
		found = _feel( expression, pre|BM_INSTANTIATED, &proxies, ctx );
		if (( found )) success = 2; }

	if ( negated ) {
		success = !success;
		_release( pre, found ); }
	else if (( mark )) switch ( success ) {
		case 1: *mark = bm_mark( pre, NULL, src, found ); break;
		case 2: *mark = bm_mark( pre, expression, src, found ); break; }
#ifdef DEBUG
	fprintf( stderr, "on_event_x end\n" );
#endif
	return success;
}

static inline void *
_test( int pre, ProxyTest *func, listItem **proxies )
{
	CNInstance *proxy;
	if ( pre & BM_AS_PER ) {
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
		return NULL; }
}
static inline void *
_feel( char *p, int type, listItem **proxies, BMContext *ctx )
{
	CNInstance *proxy;
	void *found;
	if ( type & BM_AS_PER ) {
		listItem *results = NULL;
		while (( proxy = popListItem(proxies) )) {
			found = bm_eeno_feel( proxy, type, p, ctx );
			if (( found )) {
				Pair *batch = newPair( found, proxy );
				addItem( &results, batch ); } }
		return results; }
	else {
		while (( proxy = popListItem(proxies) )) {
			found = bm_eeno_feel( proxy, type, p, ctx );
			if (( found )) {
				freeListItem( proxies );
				return newPair( found, proxy ); } }
		return NULL; }
}
static inline void
_release( int type, void *found )
{
	Pair *batch;
	if (( found )) {
		if ( type & BM_AS_PER ) {
			while (( batch=popListItem((listItem **)&found) )) {
				freeListItem((listItem **) &batch->name );
				freePair( batch ); } }
		else freePair( found ); }
}

//===========================================================================
//	do_action
//===========================================================================
static int
do_action( char *expression, BMContext *ctx, CNStory *story )
{
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
	return 1;
}

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
*/
{
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
	return retval;
}

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
	resp.	>& instead of >

	then outputs expression(s) to stdout resp. stderr
	according to fmt
*/
{
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
	return retval;
}

//===========================================================================
//	do_enable
//===========================================================================
static BMQueryCB enable_CB;
typedef struct {
	CNNarrative *narrative;
	Registry *subs;
	Pair *entry;
} EnableData;

static int
do_enable( Registry *subs, listItem *narratives, char *expression, BMContext *ctx )
{
	EnableData data;
	data.subs = subs;
	for ( listItem *i=narratives->next; i!=NULL; i=i->next ) {
		CNNarrative *narrative = i->ptr;
		// build query string "proto:expression"
		CNString *s = bm_deparameterize( narrative->proto );
		StringAppend( s, ':' );
		s_add( expression )
		char *p = StringFinish( s, 0 );
#ifdef DEBUG
		if ( !strncmp( p, "%(.,...):", 9 ) ) {
			fprintf( stderr, ">>>>> B%%: do_enable(): Warning: "
				"proto %s resolves to %%(.,...) - "
				"passes through!!\n", narrative->proto ); }
#endif
		// launch enable query
		data.narrative = narrative;
		data.entry = NULL;
		bm_query( BM_CONDITION, p, ctx, enable_CB, &data );
		freeString( s ); }
	return 1;
}
static BMCBTake
enable_CB( CNInstance *e, BMContext *ctx, void *user_data )
{
	Pair *entry;
	EnableData *data = user_data;
	if (!( entry = data->entry )) {
		entry = registryRegister( data->subs, data->narrative, NULL );
		data->entry = entry; }
	addIfNotThere((listItem **) &entry->value, e );
	return BM_CONTINUE;
}

