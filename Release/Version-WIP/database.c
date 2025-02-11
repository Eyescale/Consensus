#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "string_util.h"
#include "database.h"
#include "errout.h"

// #define DEBUG

//===========================================================================
//	newCNDB / freeCNDB
//===========================================================================
static freeRegistryCB free_CB;

CNDB *
newCNDB( CNEntity *this ) {
	CNInstance *nil = cn_new( NULL, NULL );
	Registry *index = newRegistry( IndexedByNameRef );
	return (CNDB *) newPair( nil, index ); }

void
freeCNDB( CNDB *db )
/*
	delete all CNDB entities, proceeding top down
*/ {
	if ( db == NULL ) return;
	CNInstance *nil = db->nil;
	freePair((Pair *) nil->sub );
	nil->sub = NULL;
	cn_prune( nil );
	freeRegistry( db->index, free_CB );
	freePair((Pair *) db ); }

static void
free_CB( Registry *registry, Pair *entry ) {
	free( entry->name );
	CNInstance *e = entry->value;
	e->sub = NULL;
	cn_prune( e ); }

//===========================================================================
//	new_proxy / free_proxy
//===========================================================================
CNInstance *
new_proxy( CNEntity *this, CNEntity *that, CNDB *db )
/*
	Assumptions: proxy not already created, and that!=NULL
*/ {
	CNInstance *proxy = cn_new( cn_new(this,that), NULL );
	if ( this==NULL ) db->nil->sub[ 0 ] = proxy;
	db_op( DB_MANIFEST_OP, proxy, db );
	return proxy; }

void
free_proxy( CNEntity *proxy, CNDB *db )
/*
	Assumption: e is proxy:( connection:(this,that), NULL )
	where this==NULL denotes current cell's Self proxy.
	cn_release( connection ) will remove connection
	from this->as_sub[ 0 ] and from that->as_sub[ 1 ]
*/ {
	Pair *id = (Pair *) db->nil->sub; // [ %%, .. ]
	if ( proxy!=id->name && proxy!=id->value ) {
		CNEntity *connection = proxy->sub[ 0 ];
		cn_release( proxy );
		cn_release( connection ); } }

//===========================================================================
//	db_share
//===========================================================================
CNInstance *
db_share( uint master[2], Registry *ref, CNDB *db ) {
	CNInstance *e = cn_new( NULL, NULL );
	uint *key = CNSharedKey( e );
	key[ 0 ] = master[ 0 ];
	key[ 1 ] = master[ 1 ];
	registryRegister( ref, db, e );
	db_op( DB_MANIFEST_OP, e, db );
	return e; }

//===========================================================================
//	db_register
//===========================================================================
CNInstance *
db_register( char *p, CNDB *db )
/*
	register instance represented by p
	manifest only if instance is new or rehabilitated
*/ {
	if ( p == NULL ) return NULL;
	CNInstance *e = NULL;
	Pair *entry = registryLookup( db->index, p );
	if (( entry )) {
		e = entry->value;
		db_op( DB_REHABILITATE_OP, e, db ); }
	else {
		entry = registryRegister( db->index, strmake(p), NULL );
		e = entry->value = cn_carrier( entry );
		db_op( DB_MANIFEST_OP, e, db ); }
	return e; }

//===========================================================================
//	db_deregister
//===========================================================================
void
db_deregister( CNInstance *e, CNDB *db ) {
	// remove entry from db->index
	Pair *entry = (Pair *) e->sub;
	registryRemove( db->index, entry );
	// free entry
	free( entry->name );
	freePair( entry );
	// release e
	e->sub = NULL;
	cn_release( e ); }

//===========================================================================
//	db_deprecate
//===========================================================================
static inline int deprecatable( CNInstance *e, CNDB *db );
static inline void deprecate( CNInstance *x, CNDB *db );

void
db_deprecate( CNInstance *x, CNDB *db )
/*
	deprecate (ie. set "to-be-released") x and all its ascendants,
	proceeding top-down
*/ {
	if (( x ) && deprecatable(x,db) )
		deprecate( x, db ); }

static inline void
deprecate( CNInstance *x, CNDB *db ) {
	listItem * stack = NULL,
		 * i = newItem( x ),
		 * j;
	int ndx = 0;
	for ( ; ; ) {
		x = i->ptr;
		for ( j=x->as_sub[ ndx ]; j!=NULL; j=j->next )
			if ( deprecatable( j->ptr, db ) )
				break;
		if (( j )) {
			addItem( &stack, i );
			add_item( &stack, ndx );
			i=j; ndx=0; continue; }

		if ( ndx ) db_op( DB_DEPRECATE_OP, x, db );
		else { ndx=1; continue; }
		for ( ; ; ) {
			if (( i->next )) {
				i = i->next;
				if ( deprecatable( i->ptr, db ) )
					{ ndx=0; break; } }
			else if (( stack )) {
				ndx = pop_item( &stack );
				i = popListItem( &stack );
				if ( ndx ) db_op( DB_DEPRECATE_OP, i->ptr, db );
				else { ndx=1; break; } }
			else goto RETURN; } }
RETURN:
	freeItem( i ); }

static inline int
deprecatable( CNInstance *e, CNDB *db )
/*
	returns 0 if e is either released or to-be-released,
		which we assume applies to all its ascendants
	returns 1 otherwise.
*/ {
	CNInstance *nil = db->nil;
	if ( cn_hold( e, nil ) )
		return 0;
	CNInstance *f = cn_instance( e, nil, 1 );
	return ( !f || ( f->as_sub[0] && !f->as_sub[1] )); }

//===========================================================================
//	db_signal
//===========================================================================
static inline int flareable( CNInstance *, CNDB * );
static inline int uncoupled( CNInstance *, CNInstance *, CNDB * );

void
db_signal( CNInstance *x, CNDB *db )
/*
	Assumption: x!=NULL
	if x is a flareable base entity
		flare x, ie. invoke db_op( DB_DEPRECATE_OP, x, db )
		and deprecate x's ascendants if deprecatable
	otherwise
		. deprecate x and all its ascendants
		. deprecate all x's subs so long as
			these are not referenced anywhere else
			these are not proxies
	proceeding top-down
*/ {
	if isBase( x ) {
		if ( flareable( x, db ) )
			deprecate( x, db ); }
	else if ( deprecatable( x, db ) ) {
		// deprecate x and all its ascendants
		deprecate( x, db );
		// deprecate x's subs, proceeding top-down
		listItem * stack = NULL;
		int ndx = 0;
		for ( ; ; ) {
			CNInstance *y = CNSUB( x, ndx );
			if ( uncoupled( y, x, db ) ) {
				db_op( DB_DEPRECATE_OP, y, db );
				add_item( &stack, ndx );
				addItem( &stack, x );
				x=y; ndx=0; continue; }
			while ( ndx && ( stack )) {
				x = popListItem( &stack );
				ndx = pop_item( &stack ); }
			if ( !ndx ) ndx = 1;
			else return; } } }

static inline int
flareable( CNInstance *e, CNDB *db )
/*
	Assumption: e is Base Entity (therefore cannot hold nil)
	returns 2 if e is deprecatable
	returns 1 if e is neither to-be-released nor flared
	returns 0 otherwise - which we assume applies to all e's ascendants
	Note that released entities are flareable (vs. deprecatable)
*/ {
	CNInstance *nil = db->nil;
	CNInstance *f = cn_instance( e, nil, 1 );
	if (( f )) {
		if (( f->as_sub[ 1 ] ))
			return 0; // to-be-released
		if (( f->as_sub[ 0 ] ))
			return 2; // newborn
		f = cn_instance( nil, e, 0 );
		if (( f ) && ( f->as_sub[0] ))
			return 0; }
	return 2; /* deprecatable */ }

static inline int
uncoupled( CNInstance *y, CNInstance *x, CNDB *db )
/*
	check all connections other than x to and from y=x's sub, and
	make sure that all these connections are either released or
	to-be-released - so that we can release y
*/ {
	if ( !y || cnIsProxy(y) ) // proxies are kept untouched
		return 0;
	CNInstance *nil = db->nil;
	for ( int ndx=0; ndx<2; ndx++ )
		for ( listItem *i=y->as_sub[ ndx ]; i!=NULL; i=i->next ) {
			CNInstance *e = i->ptr;
			if ( e==x || e->sub[!ndx]==nil )
				continue;
			if ( !cn_instance( e, nil, 1 ) )
				return 0; }
	return 1; }

//===========================================================================
//	db_fire
//===========================================================================
void
db_fire( CNInstance *proxy, CNDB *db )
/*
	deprecates either all e:(proxy,.) or, if dangling, proxy
*/ {
	if ( deprecatable( proxy, db ) ) {
		if (( CNProxyThat( proxy ) ))
			db_clear( proxy, 0, db );
		else deprecate( proxy, db ); } }

//===========================================================================
//	db_match / db_match_identifier
//===========================================================================
int
db_match( CNInstance *x, CNDB *db_x, CNInstance *y, CNDB *db_y )
/*
	Assumption: x!=NULL
	Note that we use y to lead the traversal - but works either way
*/ {
	if ( !y ) return 0;
	if ( db_x==db_y ) return ( x==y );
	listItem *stack = NULL;
	int ndx = 0;
	for ( ; ; ) {
		if (( CNSUB(y,ndx) )) {
			if ( !CNSUB(x,ndx) )
				goto FAIL;
			add_item( &stack, ndx );
			addItem( &stack, y );
			addItem( &stack, x );
			y = y->sub[ ndx ];
			x = x->sub[ ndx ];
			ndx = 0; continue; }

		if ( cnIsProxy(y) ) {
			if ( !cnIsProxy(x) || CNProxyThat(x)!=CNProxyThat(y) )
				goto FAIL; }
		else if ( cnIsShared(y) ) {
			if ( CNSharedKey( x )!=CNSharedKey( y ) )
				goto FAIL; }
		else if ( !cnIsIdentifier(x) )
			goto FAIL;
		else {
			char *p_x = CNIdentifier( x );
			char *p_y = CNIdentifier( y );
			if ( strcomp( p_x, p_y, 1 ) )
				goto FAIL; }
		for ( ; ; ) {
			if ( !stack ) return 1;
			x = popListItem( &stack );
			y = popListItem( &stack );
			if ( !pop_item( &stack ) )
				{ ndx=1; break; } } }
FAIL:
	freeListItem( &stack );
	return 0; }

int
db_match_identifier( CNInstance *x, char *p )
/*
	Assumption: x!=NULL
*/ {
	if ( !cnIsIdentifier(x) ) return 0;
	char *identifier = CNIdentifier( x );
	if ( !identifier ) return 0;
	switch ( *p ) {
	case '/': return !strcomp( p, identifier, 2 );
	case '\'': for ( char_s q; charscan( p+1, &q ); )
			return !strcomp( q.s, identifier, 1 );
		   return 0;
	default:
		return !strcomp( p, identifier, 1 ); } }

//===========================================================================
//	db_instantiate
//===========================================================================
static inline int concurrent( CNInstance *, CNDB * );

CNInstance *
db_instantiate( CNInstance *e, CNInstance *f, CNDB *db )
/*
	Assumption: neither e nor f are released
	Special case: assignment - i.e. e is ( *, variable )
		in which case we must deprecate current assignment
	Note that until next frame, ((*,e), . ) will exist both as
		the current (either to-be-released or reassigned) and
		the new (either newborn, reassigned, or released-to-be-manifested)
	assignments.
*/ {
#ifdef DEBUG
	db_outputf( db, stderr, "db_instantiate: ( %_, %_ )\n", e, f );
#endif
	CNInstance *instance = NULL, *candidate;
	if ( cnStarMatch( CNSUB(e,0) ) ) {
		/* Assignment case - as we have e:( *, . )
		*/
		// ward off concurrent reassignment case
		for ( listItem *i=e->as_sub[0]; i!=NULL; i=i->next ) {
			candidate = i->ptr;
			if ( concurrent( candidate, db ) ) {
				if ( candidate->sub[ 1 ]==f )
					return candidate;
				goto ERR; } }
		// deprecate previous assignment and spot reassignment
		for ( listItem *i=e->as_sub[0]; i!=NULL; i=i->next ) {
			candidate = i->ptr;
			if ( candidate->sub[ 1 ]==f )
				instance = candidate;
			else db_deprecate( candidate, db ); }
		if (( instance )) {
			db_op( DB_REASSIGN_OP, instance, db );
			return instance; } }
	else {
		/* General case
		*/
		instance = cn_instance( e, f, 0 );
		if (( instance )) {
			db_op( DB_REHABILITATE_OP, instance, db );
			return instance; } }

	instance = cn_new( e, f );
	db_op( DB_MANIFEST_OP, instance, db );
	return instance;
ERR:
	errout( DBConcurrentAssignment, db, e->sub[1], candidate->sub[1], f );
	return NULL; }

static inline int
concurrent( CNInstance *e, CNDB *db ) {
	CNInstance *nil = db->nil, *f;
	if (( f=cn_instance( e, nil, 1 ) )) {
		if (( f->as_sub[ 0 ] )) return 1; } // newborn
	if (( f=cn_instance( nil, e, 0 ) )) {
		if (( f->as_sub[ 1 ] )) return 1; } // to-be-manifested
	return 0; }

//===========================================================================
//	db_assign
//===========================================================================
CNInstance *
db_assign( CNInstance *variable, CNInstance *value, CNDB *db ) {
	CNInstance *star = db_register( "*", db );
	variable = db_instantiate( star, variable, db );
	return db_instantiate( variable, value, db ); }

//===========================================================================
//	db_unassign
//===========================================================================
CNInstance *
db_unassign( CNInstance *x, CNDB *db )
/*
	deprecate ((*,x),.) and force manifest (*,x) if it exists
	otherwise instantiate (*,x)
*/ {
	CNInstance *star = db_register( "*", db );
	CNInstance *e;
	for ( listItem *i=x->as_sub[1]; i!=NULL; i=i->next ) {
		e = i->ptr;
		if ( e->sub[0]!=star )
			continue;
		for ( listItem *j=e->as_sub[0]; j!=NULL; j=j->next ) {
			CNInstance *f = j->ptr;
			db_deprecate( f, db ); }
		db_op( DB_REASSIGN_OP, e, db );
		return e; }
	e = cn_new( star, x );
	db_op( DB_MANIFEST_OP, e, db );
	return e; }

//===========================================================================
//	db_lookup / db_scan
//===========================================================================
CNInstance *
db_lookup( int privy, char *p, CNDB *db ) {
	if ( !p ) return NULL;
	Pair *entry = registryLookup( db->index, p );
	return (( entry ) && !db_private( privy, entry->value, db )) ?
		entry->value : NULL; }

listItem *
db_rxscan( int privy, char *regex, CNDB *db ) {
	listItem *list = NULL;
	for ( listItem *i = db->index->entries; i!=NULL; i=i->next ) {
		Pair *entry = i->ptr;
		if ( !strcomp( regex, entry->name, 2 ) )
			addIfNotThere( &list, entry->value ); }
	return list; }

//===========================================================================
//	CNIdentifier, cnIsUnnamed, cnStarMatch, cnIsShared
//===========================================================================
// see database.h

//===========================================================================
//	db_traverse, DBFirst, DBNext
//===========================================================================
int
db_traverse( int privy, CNDB *db, DBTraverseCB user_CB, void *user_data )
/*
	traverses whole CNDB applying user_CB to each entity
	Note: does not traverse any %(proxy,...), Self included
*/ {
	listItem *stack = NULL;
	listItem *n = ( privy ? newItem( db->nil ) : NULL );
	listItem *i = ( privy ? n : db->index->entries );
	while (( i )) {
		CNInstance *e;
		if (( stack ) || (n))
			e = i->ptr;
		else {
			Pair *pair = i->ptr;
			e = pair->value; }
		listItem *j = e->as_sub[ 0 ];
		if (( j )) {
			addItem( &stack, i );
			i = j; continue; }
		while (( i )) {
			if ( user_CB( e, db, user_data ) ) {
				freeListItem( &n );
				freeListItem( &stack );
				return 1; }
			if (( i->next )) {
				i = i->next;
				break; }
			else if (( stack )) {
				i = popListItem( &stack );
				if ((stack) || (n))
					e = i->ptr;
				else {
					Pair *pair = i->ptr;
					e = pair->value; } }
			else if (( n )) {
				freeListItem( &n );
				i = db->index->entries;
				break; }
			else i = NULL; } }
	return 0; }

CNInstance *
DBFirst( CNDB *db, listItem **stack ) {
	for ( listItem *i=db->index->entries; i!=NULL; i=i->next ) {
		Pair *entry = i->ptr;
		CNInstance *e = entry->value;
		if ( !db_private( 0, e, db ) ) {
			addItem( stack, i );
			return e; } }
	return NULL; }

CNInstance *
DBNext( CNDB *db, CNInstance *e, listItem **stack ) {
	if ( e == NULL ) return NULL;
	for ( listItem *i=e->as_sub[0]; i!=NULL; i=i->next ) {
		e = i->ptr;
		if ( !db_private( 0, e, db ) ) {
			addItem( stack, i );
			return e; } }
	listItem *i = popListItem( stack );
	for ( ; ; ) {
		if (( i->next )) {
			i = i->next;
			if (( *stack ))
				e = i->ptr;
			else {
				// e in db->index
				Pair *entry = i->ptr;
				e = entry->value; }
			if ( !db_private( 0, e, db ) ) {
				addItem( stack, i );
				return e; } }
		else if (( *stack ))
			i = popListItem( stack );
		else return NULL; } }

//===========================================================================
//	db_outputf
//===========================================================================
static int outputf( FILE *, CNDB *, int type, CNInstance * );

int
db_outputf( CNDB *db, FILE *stream, char *fmt, ... ) {
	CNInstance *e;
	char *string;
	listItem *xpn;
	int num;
	va_list ap;
	va_start( ap, fmt );
	for ( char *p=fmt; *p; p++ )
		switch (*p) {
		case '%':
			p++;
			switch ( *p ) {
			case '%':
				fprintf( stream, "%%" );
				break;
			case 'd':
				num = va_arg( ap, int );
				fprintf( stream, "%d", num );
				break;
			case '^':
				xpn = va_arg( ap, listItem * );
				xpn_out( stream, xpn );
				break;
			case '$':
				string = va_arg( ap, char * );
				fprintf( stream, "%s", string );
				break;
			case '_':
			case 's':
				e = va_arg( ap, CNInstance * );
				outputf( stream, db, *p, e ); }
			break;
		default:
			fprintf( stream, "%c", *p ); }
	va_end( ap );
	return 0; }

static int
outputf( FILE *stream, CNDB *db, int type, CNInstance *e )
/*
	type is either 's' or '_', the difference being that,
	in case type=='s' we start non-base entity expression
	with backslash and we output SCE as-is
*/ {
	if ( e == NULL ) return 0;
	if ( type=='s' && CNSUB(e,0) )
		fprintf( stream, "\\" );

	CNInstance *nil = db->nil;
	listItem *stack = NULL;
	int ndx = 0;
	for ( ; ; ) {
		if ( e==nil )
			fprintf( stream, "(nil)" );
		else if (( CNSUB(e,ndx) )) {
			fprintf( stream, (ndx? "," : "(" ));
			add_item( &stack, ndx );
			addItem( &stack, e );
			e = e->sub[ ndx ];
			ndx=0; continue; }
		else if ( cnIsProxy(e) ) {
			fprintf( stream, "%s", cnIsSelf(e)?"%%":"@@@" ); }
		else if ( cnIsShared(e) ) {
			if ( cnIsUnnamed(e) ) fprintf( stream, "!!" );
			else {
				char *p = db_arena_identifier( e );
				if ( type=='s' ) fprintf( stream, "%s", p );
				else for ( ; *p; p++ )
					switch ( *p ) {
					case '\n': fprintf( stream, "\\n" ); break;
					case '\t': fprintf( stream, "\\t" ); break;
					case '\\': fprintf( stream, "\\\\" ); break;
					default: fprintf( stream, "%c", *p ); } } }
		else {
			char *p = CNIdentifier( e );
			if ( type=='s' || *p=='*' || *p=='%' || !is_separator(*p) )
				fprintf( stream, "%s", p );
			else {
				switch (*p) {
				case '\0': fprintf( stream, "'\\0'" ); break;
				case '\t': fprintf( stream, "'\\t'" ); break;
				case '\n': fprintf( stream, "'\\n'" ); break;
				case '\'': fprintf( stream, "'\\''" ); break;
				case '\\': fprintf( stream, "'\\\\'" ); break;
				default:
					if ( is_printable(*p) )
						fprintf( stream, "'%c'", *p );
					else fprintf( stream, "'\\x%.2X'", *(unsigned char *)p );
				} } }
		for ( ; ; ) {
			if (( stack )) {
				e = popListItem( &stack );
				if (( ndx = pop_item( &stack ) )) {
					fprintf( stream, ")" ); }
				else { ndx=1; break; } }
			else return 0; } } }

