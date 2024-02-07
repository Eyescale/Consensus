#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "context.h"

//===========================================================================
//	bm_locate_mark, bm_locate_param
//===========================================================================
typedef void BMLocateCB( char *, listItem *, void * );
static char *bm_locate_param( char *, listItem **, BMLocateCB, void * );

char *
bm_locate_mark( char *expression, listItem **exponent )
{
	return bm_locate_param( expression, exponent, NULL, NULL );
}
static char *
bm_locate_param( char *expression, listItem **exponent, BMLocateCB arg_CB, void *user_data )
/*
	if arg_CB is set
		invokes arg_CB on each .arg found in expression
	Otherwise
		returns first '?' found in expression, together with exponent
	Note that we ignore the '~' signs and %(...) sub-expressions
	Note also that the caller is expected to reorder the list of exponents
*/
{
	struct { listItem *couple, *level; } stack = { NULL, NULL };
	listItem *level = NULL;
	int	scope = 0,
		done = 0,
		couple = 0; // base is singleton
	char *p = expression;
	while ( *p && !done ) {
		switch ( *p ) {
		case '~':
		case '%':
			p = p_prune( PRUNE_FILTER, p+1 );
			break;
		case '*':
			if (( arg_CB )) {
				p = p_prune( PRUNE_FILTER, p+1 );
				break;
			}
			if ( p[1] && !strmatch( ":,)", p[1] ) ) {
				xpn_add( exponent, AS_SUB, 1 );
				xpn_add( exponent, SUB, 0 );
				xpn_add( exponent, SUB, 1 );
			}
			p++; break;
		case '(':
			scope++;
			add_item( &stack.couple, couple );
			if (( couple = !p_single( p ) ))
				xpn_add( exponent, SUB, 0 );
			addItem( &stack.level, level );
			level = *exponent;
			p++; break;
		case ':':
			while ( *exponent != level )
				popListItem( exponent );
			p++; break;
		case ',':
			if ( !scope ) { done=1; break; }
			while ( *exponent != level ) {
				popListItem( exponent );
			}
			xpn_set( *exponent, SUB, 1 );
			p++; break;
		case ')':
			if ( !scope ) { done=1; break; }
			scope--;
			if ( couple ) {
				while ( *exponent != level ) {
					popListItem( exponent );
				}
				popListItem( exponent );
			}
			level = popListItem( &stack.level );
			couple = pop_item( &stack.couple );
			p++; break;
		case '?':
			if (( arg_CB )) p++;
			else done = 2;
			break;
		case '.':
			p++;
			if ( arg_CB && !is_separator(*p) )
	 			arg_CB( p, *exponent, user_data );
			// no break
		default:
			p = p_prune( PRUNE_IDENTIFIER, p );
		}
	}
	if ( scope ) {
		freeListItem( &stack.couple );
		freeListItem( &stack.level );
	}
	return ((*p=='?') ? p : NULL );
}

//===========================================================================
//	newContext / freeContext
//===========================================================================
typedef struct {
	CNInstance *e;
	Registry *registry;
} RegisterData;
static BMLocateCB arg_register_CB;

BMContext *
newContext( CNNarrative *n, CNInstance *e, CNDB *db )
{
	Registry *registry = newRegistry( IndexedByNameRef );
	registryRegister( registry, "?", NULL ); // aka. %?
	registryRegister( registry, "!", NULL ); // aka. %!
	if (( e )) {
		registryRegister( registry, "this", e );
		if ((n) && (n->proto)) {
			listItem *exponent = NULL;
			RegisterData data = { e, registry };
			bm_locate_param( n->proto, &exponent, arg_register_CB, &data );
		}
	}
	return (BMContext *) newPair( db, registry );
}
static void
arg_register_CB( char *p, listItem *exponent, void *user_data )
{
	RegisterData *data = user_data;
	listItem *xpn = NULL;
	for ( listItem *i=exponent; i!=NULL; i=i->next )
		addItem( &xpn, i->ptr );
	int exp;
	CNInstance *x = data->e;
	while (( exp = pop_item( &xpn ) ))
		x = x->sub[ exp & 1 ];
	registryRegister( data->registry, p, x );
}
void
freeContext( BMContext *ctx )
{
	Registry *registry = ctx->registry;
	if (( registry )) {
		freeRegistry( registry, NULL );
	}
	freePair((Pair *) ctx );
}

//===========================================================================
//	bm_push_mark / bm_pop_mark / lookup_mark_register
//===========================================================================
listItem *
bm_push_mark( BMContext *ctx, int mark, void *e )
{
	Pair *entry = registryLookup( ctx->registry, ((char_s) mark).s );
	return ( entry ) ? addItem((listItem**)&entry->value, e ) : NULL;
}
void *
bm_pop_mark( BMContext *ctx, int mark )
{
	Pair *entry = registryLookup( ctx->registry, ((char_s) mark).s );
	return ( entry ) ? popListItem((listItem**)&entry->value) : NULL;
}
void *
lookup_mark_register( BMContext *ctx, int mark )
{
	Pair *entry = registryLookup( ctx->registry, ((char_s) mark).s );
	return (entry) && (entry->value) ? ((listItem*)entry->value)->ptr : NULL;
}

//===========================================================================
//	bm_lookup
//===========================================================================
CNInstance *
bm_lookup( int privy, char *p, BMContext *ctx )
{
	Pair *entry;
	char_s q;
	switch ( *p ) {
	case '%': // looking up %? or %!
		return strmatch( "?!", p[1] ) ? lookup_mark_register( ctx, p[1] ) :
			db_lookup( privy, p, ctx->db );
	case '\'': // looking up single character identifier instance
		return ( charscan( p+1, &q ) ) ?
			db_lookup( privy, q.s, ctx->db ) : NULL;
	default: // looking up normal identifier instance
		if ( !is_separator(*p) ) {
			entry = registryLookup( ctx->registry, p );
			if (( entry )) return entry->value;
		}
		return db_lookup( privy, p, ctx->db );
	}
}

//===========================================================================
//	bm_register
//===========================================================================
CNInstance *
bm_register( BMContext *ctx, char *p )
{
	Registry *registry = ctx->registry;
	CNDB *db = ctx->db;
	Pair *entry;
	char_s q;
	switch ( *p ) {
	case '.': // registering .local(s)
		entry = registryLookup( registry, "this" );
		CNInstance *this = !entry ? // base narrative
			db_register( "this", db, 1 ) : entry->value;
		for ( ; ; p=p_prune( PRUNE_IDENTIFIER, p )) {
			for ( ; *p!='.' || is_separator(p[1]); p++ )
				if ( *p == '\0' ) return NULL;
			p++;
			entry = registryLookup( registry, p );
			if (( entry )) {
				CNInstance *e = entry->value;
				if ( e->sub[ 0 ] == this )
					continue; // already registered
			}
			CNInstance *x = db_register( p, db, 0 );
			x = db_couple( this, x, db );
			if (( entry )) { // was .arg
				entry->name = p;
				entry->value = x;
			}
			else registryRegister( registry, p, x );
		}
		break;
	case '\'': // registering single character identifier instance
		if ( charscan( p+1, &q ) )
			return db_register( q.s, db, 1 );
		break;
	default: // registering normal identifier instance
		if ( !is_separator(*p) ) {
			entry = registryLookup( registry, p );
			if (( entry )) return entry->value;
		}
		return db_register( p, db, 1 );
	}
	return NULL;
}

//===========================================================================
//	xpn_add, xpn_set
//===========================================================================
void
xpn_add( listItem **xp, int as_sub, int position )
{
	union { int value; void *ptr; } icast;
	icast.value = as_sub + position;
	addItem( xp, icast.ptr );
}
void
xpn_set( listItem *xp, int as_sub, int position )
{
	union { int value; void *ptr; } icast;
	icast.value = as_sub + position;
	xp->ptr = icast.ptr;
}

//===========================================================================
//	xpn_out
//===========================================================================
void
xpn_out( FILE *stream, listItem *xp )
{
	while ( xp ) {
		fprintf( stream, "%d", (int)xp->ptr );
		xp = xp->next;
	}
}

