#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "cell.h"
#include "operation.h"
#include "deternarize.h"
#include "expression.h"
#include "instantiate.h"
#include "proxy.h"

// #define DEBUG

static int in_condition( char *, BMContext *, int *marked );
static int on_event( char *, BMContext *, int *marked );
static int on_event_x( char *, BMContext *, int *marked );
static int do_action( char *, BMContext *, CNStory *story );
static int do_enable( Registry *, listItem *, char *, BMContext * );
static int do_input( char *, BMContext * );
static int do_output( char *, BMContext * );

//===========================================================================
//	bm_operate
//===========================================================================
#define ctrl(e)	case e:	if ( passed ) { j = NULL; break; }

void
bm_operate( CNNarrative *narrative, CNInstance *instance, BMContext *ctx,
	Registry *subs, listItem *narratives, CNStory *story )
{
#ifdef DEBUG
	fprintf( stderr, "operate bgn\n" );
#endif
	bm_context_set( ctx, narrative->proto, instance );
	listItem *i = newItem( narrative->root ), *stack = NULL;
	int passed = 1, marked = 0;
	for ( ; ; ) {
		CNOccurrence *occurrence = i->ptr;
		char *expression = occurrence->data->expression;
		char *deternarized = NULL;
		listItem *j = occurrence->sub;
		switch ( occurrence->data->type ) {
		ctrl(ELSE) case ROOT:
			passed = 1;
			break;
		ctrl(ELSE_IN) case IN:
			deternarized = bm_deternarize( &expression, ctx );
			passed = in_condition( expression, ctx, &marked );
			break;
		ctrl(ELSE_ON) case ON:
			deternarized = bm_deternarize( &expression, ctx );
			passed = on_event( expression, ctx, &marked );
			break;
		ctrl(ELSE_ON_X) case ON_X:
			deternarized = bm_deternarize( &expression, ctx );
			passed = on_event_x( expression, ctx, &marked );
			break;
		ctrl(ELSE_DO) case DO:
			deternarized = bm_deternarize( &expression, ctx );
			do_action( expression, ctx, story );
			break;
		ctrl(ELSE_EN) case EN:
			deternarized = bm_deternarize( &expression, ctx );
			do_enable( subs, narratives, expression, ctx );
			break;
		ctrl(ELSE_INPUT) case INPUT:
			deternarized = bm_deternarize( &expression, ctx );
			do_input( expression, ctx );
			break;
		ctrl(ELSE_OUTPUT) case OUTPUT:
			deternarized = bm_deternarize( &expression, ctx );
			do_output( expression, ctx );
			break;
		case LOCALE:
			bm_declare( ctx, expression );
			break; }

		if (( deternarized )) free( expression );
		if (( j && passed )) {
			addItem( &stack, i );
			add_item( &stack, marked );
			i = j; marked = 0;
			continue; }
		for ( ; ; ) {
			if (( marked )) {
				bm_context_unmark( ctx, marked );
				marked = 0; }
			if (( i->next )) {
				i = i->next;
				break; }
			else if (( stack )) {
				passed = 1; // otherwise we would not be here
				marked = pop_item( &stack );
				i = popListItem( &stack ); }
			else goto RETURN; } }
RETURN:
	freeItem( i );
	bm_context_clear( ctx );
#ifdef DEBUG
	fprintf( stderr, "operate end\n" );
#endif
}

//===========================================================================
//	in_condition
//===========================================================================
static int
in_condition( char *expression, BMContext *ctx, int *marked )
/*
	Assumption: *marked==0 to begin with
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
	else if ( success )
		bm_context_mark( ctx, expression, found, marked );
#ifdef DEBUG
	fprintf( stderr, "in_condition end\n" );
#endif
	return success;
}

//===========================================================================
//	on_event
//===========================================================================
static int
on_event( char *expression, BMContext *ctx, int *marked )
/*
	Assumption: *marked==0 to begin with
*/
{
#ifdef DEBUG
	fprintf( stderr, "on_event bgn: %s\n", expression );
#endif
	int success=0, negated=0;
	if ( !strncmp(expression,"~.:",3) )
		{ negated=1; expression+=3; }

	CNInstance *found;
	if ( !is_separator(*expression) ) {
		if ( !strcmp( expression, "init" ) ) {
			CNDB *db = BMContextDB( ctx );
			success = DBInitOn( db ); }
		else {
			char *p = expression;
			do p++; while ( !is_separator(*p) );
			switch ( *p ) {
			case ':':
				found = bm_feel( BM_INSTANTIATED, expression, ctx );
				if (( found )) { expression=p+1; success=2; }
				break;
			default:
				found = ( *p=='~' ) ?
					bm_feel( BM_RELEASED, expression, ctx ) :
					bm_feel( BM_INSTANTIATED, expression, ctx );
				if (( found )) success = 1; } } }
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
	else if ( success==2 )
		bm_context_mark( ctx, expression, found, marked );
#ifdef DEBUG
	fprintf( stderr, "on_event end\n" );
#endif
	return success;
}

//===========================================================================
//	on_event_x
//===========================================================================
typedef int ProxyTest( CNInstance *proxy );
static inline Pair * proxy_feel( BMQueryType, char *, listItem **, BMContext * );
static inline Pair * proxy_test( ProxyTest *, listItem ** );

static int
on_event_x( char *expression, BMContext *ctx, int *marked )
/*
	Assumption: *marked==0 to begin with
*/
{
#ifdef DEBUG
	fprintf( stderr, "on_event_x bgn: %s\n", expression );
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
		found = proxy_test( bm_proxy_in, &proxies );
		if (( found )) success = 1; }
	else if ( !strncmp( expression, "exit<", 5 ) ) {
		found = proxy_test( bm_proxy_out, &proxies );
		if (( found )) success = 1; }
	else if ( !strncmp(expression,".<",2) ) {
		found = proxy_test( bm_proxy_active, &proxies );
		if (( found )) success = 1; }
	else if ( !strncmp(expression,"~(",2) ) {
		expression++;
		found = proxy_feel( BM_RELEASED, expression, &proxies, ctx );
		if (( found )) success = 2; }
	else {
		found = proxy_feel( BM_INSTANTIATED, expression, &proxies, ctx );
		if (( found )) success = 2; }

	freeListItem( &proxies );
	if ( negated ) success = !success;
	else switch ( success ) {
		case 1: bm_context_mark_x( ctx, NULL, src, found, marked ); break;
		case 2: bm_context_mark_x( ctx, expression, src, found, marked ); }
	if (( found )) freePair( found );
#ifdef DEBUG
	fprintf( stderr, "on_event_x end\n" );
#endif
	return success;
}

static inline Pair *
proxy_feel( BMQueryType type, char *p, listItem **proxies, BMContext *ctx )
{
	CNInstance *proxy, *found;
	while (( proxy = popListItem(proxies) )) {
		found = bm_proxy_feel( proxy, type, p, ctx );
		if (( found )) return newPair( found, proxy ); }
	return NULL;
}
static inline Pair *
proxy_test( ProxyTest *test, listItem **proxies )
{
	CNInstance *proxy;
	while (( proxy = popListItem(proxies) ))
		if ( test( proxy ) )
			return newPair( NULL, proxy );
	return NULL;
}

//===========================================================================
//	do_action
//===========================================================================
static int
do_action( char *expression, BMContext *ctx, CNStory *story )
{
#ifdef DEBUG
	fprintf( stderr, "do_action: do %s\n", expression );
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
	CNString *s = newString();
	for ( listItem *i=narratives->next; i!=NULL; i=i->next ) {
		CNNarrative *narrative = i->ptr;
		// build query string "proto:expression"
		for ( char *p=narrative->proto; *p; p++ ) {
			// reducing proto params as we go
			if ( *p=='.' ) {
				do p++; while ( !is_separator(*p) );
				if ( *p==':' ) continue;
				else StringAppend( s, '.' ); }
			StringAppend( s, *p ); }
		if ( strcmp(expression,"%(.)") ) {
			StringAppend( s, ':' );
			for ( char *p=expression; *p; p++ )
				StringAppend( s, *p ); }
		char *q = StringFinish( s, 0 );
		// launch query
		data.narrative = narrative;
		data.entry = NULL;
		bm_query( BM_CONDITION, q, ctx, enable_CB, &data );
		// reset
		StringReset( s, CNStringAll ); }
	freeString( s );
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

	then outputs expression(s) to stdout according to fmt
*/
{
#ifdef DEBUG
	fprintf( stderr, "do_output bgn: %s\n", expression );
#endif
	// extracts fmt and args:{ expression(s) }
	char *p = expression + 1; // skipping the leading '>'
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
	int retval = bm_outputf( fmt, args, ctx );

	// cleanup
	freeListItem( &args );
#ifdef DEBUG
	fprintf( stderr, "do_output end\n" );
#endif
	return retval;
}

