#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "context.h"


//===========================================================================
//	bm_push / bm_pop
//===========================================================================
typedef void BMLocateCB( char *, listItem *, void * );
static char *bm_locate_param( char *, listItem **, BMLocateCB, void * );
typedef struct {
	CNInstance *e;
	Registry *registry;
} RegisterData;
static BMLocateCB arg_register_CB;

BMContext *
bm_push( CNNarrative *n, CNInstance *e, CNDB *db )
{
	Registry *registry = newRegistry( IndexedByCharacter );
	registryRegister( registry, "?", NULL ); // aka. %?
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
static char *
bm_locate_param( char *expression, listItem **exponent, BMLocateCB arg_CB, void *user_data )
/*
	if arg_CB is set
		invokes arg_CB on each .arg found in expression
	Otherwise
		returns first '?' found in expression, together with exponent
	Note that we ignore the '~' signs and %(...) sub-expressions
	Note also that the caller is expected to reorder the list
*/
{
	union { int value; void *ptr; } icast;
	struct {
		listItem *couple;
		listItem *level;
	} stack = { NULL, NULL };
	listItem *level = NULL;

	int	scope = 1,
		couple = 0; // default is singleton

	char *p = expression;
	while ( *p && scope ) {
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
			icast.value = couple;
			addItem( &stack.couple, icast.ptr );
			couple = !p_single( p );
			if ( couple ) {
				xpn_add( exponent, SUB, 0 );
			}
			addItem( &stack.level, level );
			level = *exponent;
			p++; break;
		case ':':
			while ( *exponent != level )
				popListItem( exponent );
			p++; break;
		case ',':
			if ( scope == 1 ) { scope=0; break; }
			while ( *exponent != level )
				popListItem( exponent );
			popListItem( exponent );
			xpn_add( exponent, SUB, 1 );
			p++; break;
		case ')':
			scope--;
			if ( !scope ) break;
			if ( couple ) {
				while ( *exponent != level )
					popListItem( exponent );
				popListItem( exponent );
			}
			level = popListItem( &stack.level );
			couple = (int) popListItem( &stack.couple );
			p++; break;
		case '?':
			if (( arg_CB )) p++;
			else scope = 0;
			break;
		case '.':
			p++;
			if ( arg_CB && !is_separator(*p) )
	 			arg_CB( p, *exponent, user_data );
			break;
		default:
			p = p_prune( PRUNE_IDENTIFIER, p );
		}
	}
	freeListItem( &stack.couple );
	freeListItem( &stack.level );
	return ((*p=='?') ? p : NULL );
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
	while (( exp = (int) popListItem( &xpn ) ))
		x = x->sub[ exp & 1 ];
	registryRegister( data->registry, p, x );
}

void
bm_pop( BMContext *ctx )
{
	Registry *registry = ctx->registry;
	if (( registry )) {
		freeRegistry( registry, NULL );
	}
	freePair((Pair *) ctx );
}

//===========================================================================
//	bm_locate_mark
//===========================================================================
char *
bm_locate_mark( char *expression, listItem **exponent )
{
	return bm_locate_param( expression, exponent, NULL, NULL );
}

//===========================================================================
//	bm_push_mark / bm_pop_mark / lookup_mark_register
//===========================================================================
listItem *
bm_push_mark( BMContext *ctx, CNInstance *e )
{
	Pair *entry = registryLookup( ctx->registry, "?" );
	return ( entry ) ? addItem((listItem**)&entry->value, e ) : NULL;
}
CNInstance *
bm_pop_mark( BMContext *ctx )
{
	Pair *entry = registryLookup( ctx->registry, "?" );
	return ( entry ) ? popListItem((listItem**)&entry->value) : NULL;
}
CNInstance *
lookup_mark_register( BMContext *ctx )
{
	Pair *entry = registryLookup( ctx->registry, "?" );
	return (entry) && (entry->value) ? ((listItem*)entry->value)->ptr : NULL;
}

//===========================================================================
//	bm_lookup
//===========================================================================
CNInstance *
bm_lookup( int privy, char *p, BMContext *ctx )
{
	Pair *entry;
	char q[ MAXCHARSIZE + 1 ];
	switch ( *p ) {
	case '%': // looking up %? or %
		return ( p[1] == '?' ) ? lookup_mark_register( ctx ) :
			db_lookup( privy, p, ctx->db );
	case '\'': // looking up single character identifier instance
		return ( charscan( p+1, q ) ) ?
			db_lookup( privy, q, ctx->db ) : NULL;
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
	Pair *entry;
	switch ( *p ) {
	case '.': // registering .local(s)
		entry = registryLookup( registry, "this" );
		if ( entry == NULL ) return NULL; // base narrative

		CNDB *db = ctx->db;
		CNInstance *this = entry->value;
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
			/* assuming previous entry was registered
			   either as .arg or not at all
			*/
			CNInstance *x = db_register( p, db, 0 );
			x = db_couple( this, x, db );
			if (( entry )) {
				entry->name = p;
				entry->name = x;
			}
			else registryRegister( registry, p, x );
		}
	case '\'':; // registering single character identifier instance
		char q[ MAXCHARSIZE + 1 ];
		if ( charscan(p+1,q) )
			return db_register( q, ctx->db, 1 );
		break;
	default: // registering normal identifier instance
		if ( !is_separator(*p) ) {
			entry = registryLookup( registry, p );
			if (( entry )) return entry->value;
		}
		return db_register( p, ctx->db, 1 );
	}
	return NULL;
}

//===========================================================================
//	xpn_add
//===========================================================================
void
xpn_add( listItem **xp, int as_sub, int position )
{
	union { int value; void *ptr; } icast;
	icast.value = as_sub + position;
	addItem( xp, icast.ptr );
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

