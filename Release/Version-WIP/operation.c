#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "program.h"
#include "operation.h"
#include "expression.h"
#include "locate.h"
#include "query.h"

// #define DEBUG

static int in_condition( char *, BMContext *, int *marked );
static int on_event( char *, BMContext *, int *marked );
static int do_action( char *, BMContext *, CNStory *story );
static int do_enable( Registry *, listItem *, char *, BMContext * );
static int do_input( char *, BMContext * );
static int do_output( char *, BMContext * );

//===========================================================================
//	operate
//===========================================================================
#define ctrl(e)	case e:	if ( passed ) { j = NULL; break; }

static void
operate( CNNarrative *narrative, CNInstance *instance, Registry *registry,
	BMContext *ctx, listItem *narratives, CNStory *story )
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
		ctrl(ELSE_DO) case DO:
			deternarized = bm_deternarize( &expression, ctx );
			do_action( expression, ctx, story );
			break;
		ctrl(ELSE_EN) case EN:
			deternarized = bm_deternarize( &expression, ctx );
			do_enable( registry, narratives, expression, ctx );
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
			bm_context_register( ctx, expression );
			break;
		}
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
			else goto RETURN;
		}
	}
RETURN:
	freeItem( i );
	bm_context_flush( ctx );
#ifdef DEBUG
	fprintf( stderr, "operate end\n" );
#endif
}

//===========================================================================
//	bm_operate
//===========================================================================
static void enlist( Registry *subs, Registry *index, Registry *warden );
static void relieve_CB( Registry *, Pair *entry );

int
bm_operate( CNCell *cell, listItem **new, CNStory *story )
{
#ifdef DEBUG
	fprintf( stderr, "bm_operate: bgn\n" );
#endif
	BMContext *ctx = cell->ctx;
	freeListItem( BMContextCarry(ctx) );
	CNDB *db = BMContextDB( ctx );
	if ( db_out(db) ) return 0;

	Registry *warden, *index[ 2 ], *swap;
	warden = newRegistry( IndexedByAddress );
	index[ 0 ] = newRegistry( IndexedByAddress );
	index[ 1 ] = newRegistry( IndexedByAddress );

	listItem *narratives = cell->entry->value;
	CNNarrative *base = narratives->ptr; // base narrative
	registryRegister( index[ 1 ], base, newItem( NULL ) );
	do {
		swap = index[ 0 ];
		index[ 0 ] = index[ 1 ];
		index[ 1 ] = swap;
		listItem **active = &index[ 0 ]->entries; // narratives to be operated
		for ( Pair *entry;( entry = popListItem(active) ); freePair(entry) ) {
			CNNarrative *narrative = entry->name;
			for ( listItem **instances = (listItem **) &entry->value; (*instances); ) {
				CNInstance *instance = popListItem( instances );
				Registry *subs = newRegistry( IndexedByAddress );
				operate( narrative, instance, subs, ctx, narratives, story );
				enlist( index[ 1 ], subs, warden );
				freeRegistry( subs, NULL ); } }
	} while (( index[ 1 ]->entries ));
	freeRegistry( warden, relieve_CB );
	freeRegistry( index[ 0 ], NULL );
	freeRegistry( index[ 1 ], NULL );
#ifdef DEBUG
	fprintf( stderr, "bm_operate: end\n" );
#endif
	return 1;
}
static void
relieve_CB( Registry *warden, Pair *entry )
{
	freeListItem((listItem **) &entry->value );
}
static void
enlist( Registry *index, Registry *subs, Registry *warden )
/*
	enlist subs:%{[ narrative, instance:%{} ]} into index of same type,
	under warden supervision: we rely on registryRegister() to return
	the found entry if there is one, and on addIfNotThere() to return
	NULL is there is none.
*/
{
#define s_place( s, index, narrative ) \
	if (!( s )) { \
		Pair *entry = registryRegister( index, narrative, NULL ); \
		s = (listItem **) &entry->value; }

	listItem **narratives = (listItem **) &subs->entries;
	for ( Pair *sub;( sub = popListItem(narratives) ); freePair(sub) ) {
		CNNarrative *narrative = sub->name;
		listItem **selection = NULL;
 		Pair *found = registryLookup( warden, narrative );
		if (!( found )) {
			registryRegister( warden, narrative, sub->value );
			s_place( selection, index, narrative );
			for ( listItem *i=sub->value; i!=NULL; i=i->next )
				addItem( selection, i->ptr ); }
		else {
			listItem **enlisted = (listItem **) &found->value;
			listItem **candidates = (listItem **) &sub->value;
			for ( CNInstance *instance;( instance = popListItem(candidates) ); ) {
				if (( addIfNotThere( enlisted, instance ) )) {
					s_place( selection, index, narrative );
					addItem( selection, instance );
		} } } }
}

//===========================================================================
//	bm_update
//===========================================================================
void
bm_update( BMContext *ctx )
{
	CNDB *db = BMContextDB( ctx );
	db_update( db );
}

//===========================================================================
//	in_condition
//===========================================================================
static int
in_condition( char *expression, BMContext *ctx, int *marked )
/*
	Assumption: *marked = 0 to begin with
*/
{
#ifdef DEBUG
	fprintf( stderr, "in condition bgn: %s\n", expression );
#endif
	CNInstance *found;
	int success, negated=0;
	if ( !strncmp( expression, "~.:", 3 ) )
		{ negated=1; expression += 3; }

	if ( !strcmp( expression, "~." ) )
		{ success=0; goto RETURN; }

	found = bm_feel( BM_CONDITION, expression, ctx );
	success = bm_context_mark( ctx, expression, found, marked );
RETURN:
#ifdef DEBUG
	fprintf( stderr, "in_condition end\n" );
#endif
	return ( negated ? !success : success );
}

//===========================================================================
//	on_event
//===========================================================================
static int
on_event( char *expression, BMContext *ctx, int *marked )
/*
	Assumption: *marked = 0 to begin with
*/
{
#ifdef DEBUG
	fprintf( stderr, "on_event bgn: %s\n", expression );
#endif
	CNInstance *found;
	int success, negated=0;
	if ( !strncmp( expression, "~.:", 3 ) )
		{ negated=1; expression += 3; }

	switch ( *expression ) {
	case '~':
		switch ( expression[1] ) {
		case '(':
			expression++;
			found = bm_feel( BM_RELEASED, expression, ctx );
			success = bm_context_mark( ctx, expression, found, marked );
			goto RETURN;
		case '.':
			success = db_still( BMContextDB(ctx) );
			goto RETURN; }
		break;
	default:
		if ( !strcmp( expression, "init" ) ) {
			success = db_in( BMContextDB(ctx) );
			goto RETURN; } }

	found = bm_feel( BM_INSTANTIATED, expression, ctx );
	success = bm_context_mark( ctx, expression, found, marked );
RETURN:
#ifdef DEBUG
	fprintf( stderr, "on_event end\n" );
#endif
	return ( negated ? !success : success );
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
	switch ( *expression ) {
	case '~':
		switch ( expression[1] ) {
		case '(':
			bm_release( expression+1, ctx );
			break;
		case '<':
			bm_proxy_op( expression, ctx );
			break; }
		goto RETURN;
	case '@':
		if ( expression[1]=='<' )
			bm_proxy_op( expression, ctx );
		goto RETURN;
	default:
		if ( !strcmp( expression, "exit" ) ) {
			db_exit( BMContextDB(ctx) );
			goto RETURN; } }

	bm_instantiate( expression, ctx, story );
RETURN:
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
				StringAppend( s, '.' ); }
			StringAppend( s, *p );
		}
		StringAppend( s, ':' );
		for ( char *p=expression; *p; p++ )
			StringAppend( s, *p );
		char *q = StringFinish( s, 0 );
		// launch query
		data.narrative = narrative;
		data.entry = NULL;
		bm_query( BM_CONDITION, q, ctx, enable_CB, &data );
		// reset
		StringReset( s, CNStringAll );
	}
	freeString( s );
	return 1;
}
static BMCBTake
enable_CB( CNInstance *e, BMContext *ctx, void *user_data )
{
	EnableData *data = user_data;
	Pair *entry;
	if (!( entry = data->entry )) {
		entry = registryRegister( data->subs, data->narrative, NULL );
		data->entry = entry;
	}
	addIfNotThere((listItem **) &entry->value, e );
	return BM_CONTINUE;
}

//===========================================================================
//	do_input
//===========================================================================
static int
do_input( char *expression, BMContext *ctx )
/*
	Assuming expression is in the form e.g.
		args : fmt <
	reads input from stdin according to fmt
*/
{
#ifdef DEBUG
	fprintf( stderr, "do_input bgn: %s\n", expression );
#endif
	CNDB *db = BMContextDB( ctx );
	if ( db_out(db) ) return 0;

	// extract fmt and args:{ expression(s) }
	char *p=expression, *q=p_prune( PRUNE_FILTER, p );
	for ( ; *q==':'; q=p_prune( PRUNE_FILTER, p ) )
		p = q+1;
	char *fmt = ( *p=='"' ? p : "" );
	q = p-1;

	CNString *s = newString();
	for ( p=expression; p!=q; p++ )
		StringAppend( s, *p );
	p = StringFinish( s, 0 );
	StringReset( s, CNStringMode );
	freeString( s );

	listItem *args = newItem( p );

	// invoke bm_inputf()
	int retval = bm_inputf( fmt, args, ctx ) ;

	// cleanup
	while (( p = popListItem( &args ) ))
		free( p );
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
	Assuming expression is in the form
		> fmt : expression(s)
	then outputs expression(s) to stdout according to fmt
*/
{
#ifdef DEBUG
	fprintf( stderr, "do_output bgn: %s\n", expression );
#endif
	expression++; // skipping the leading '>'

	// extracts fmt and args:{ expression(s) }
	char *p = expression;
	char *fmt = ( *p=='"' ? p : "" );
	if ( *fmt ) p = p_prune( PRUNE_FILTER, fmt );
	if ( *p==':') p++;
	listItem *args = ( *p ? newItem(p) : NULL );

	// invoke bm_outputf()
	int retval = bm_outputf( fmt, args, ctx );

	// cleanup
	freeListItem( &args );
#ifdef DEBUG
	fprintf( stderr, "do_output end\n" );
#endif
	return retval;
}

