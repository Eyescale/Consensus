#ifndef QUERY_PRIVATE_H
#define QUERY_PRIVATE_H

//---------------------------------------------------------------------------
//	ifn_PIVOT
//---------------------------------------------------------------------------
typedef int XPTraverseCB( CNInstance *, char *, BMQueryData * );
static CNInstance * xp_traverse_list( char *, BMQueryData *, XPTraverseCB *, void * );
static CNInstance * xp_traverse( char *, BMQueryData *, XPTraverseCB *, void * );
static inline char * pivot_check( char *, listItem **, listItem **, BMQueryData * );

#define XP_TRAVERSE( a, b, c, d ) \
	success = ((b->list) ? xp_traverse_list(a,b,c,d) : xp_traverse(a,b,c,d))
#define XP_TRAVERSE_ASSIGNMENT( a, b, c, d ) \
	if (( XP_TRAVERSE(a,b,c,d) )) success = data->instance;
#define _XP_TRAVERSE XP_TRAVERSE
#define ifn_PIVOT( privy, expression, data, CB, user_data ) \
	listItem *exponent = NULL, *xpn = NULL; \
	char *p = bm_locate_pivot( expression, &exponent ); \
	if (( p = pivot_check( p, &exponent, &xpn, data ) )) { \
		e = bm_lookup( privy, p, ctx, db ); \
		if ( !e ) { \
			freeListItem( &exponent ); \
			freeListItem( &xpn ); \
			return NULL; } \
		data->exponent = exponent; \
		data->pivot = newPair( p, e ); \
		_XP_TRAVERSE( expression, data, CB, user_data ); \
		freePair( data->pivot ); \
		freeListItem( &exponent ); \
		freeListItem( &xpn ); } \
	else

//---------------------------------------------------------------------------
//	PUSH and PUSH_LIST
//---------------------------------------------------------------------------
#define PUSH( stack, exponent, BREAK ) \
	while (( exponent )) { \
		int exp = (int) exponent->ptr; \
		if ( exp & SUB ) { \
			e = CNSUB( e, exp&1 ); \
			if (( e )) { \
				addItem( &stack, i ); \
				addItem( &stack, exponent ); \
				exponent = exponent->next; \
				i = newItem( e ); } \
			else goto BREAK; } \
		else { \
			for ( j=e->as_sub[ exp & 1 ]; j!=NULL; j=j->next ) \
				if ( !db_private( privy, j->ptr, db ) ) \
					break; \
			if (( j )) { \
				addItem( &stack, i ); \
				addItem( &stack, exponent ); \
				exponent = exponent->next; \
				i = j; e = j->ptr; } \
			else goto BREAK; } }

#define PUSH_LIST( stack, BREAK )  { \
	int empty = 1; \
	while (( j=e->as_sub[ 0 ] )) { \
		CNInstance *f = j->ptr; \
		if ( f->sub[ 0 ]==nil ) \
			break; \
		empty = 0; /* done */ \
		addItem( &stack, i ); \
		i = j; e = f; } \
	if ( empty ) /* failed */ \
		goto BREAK; \
	e = e->sub[ 1 ]; }

//---------------------------------------------------------------------------
//	POP and POP_LIST
//---------------------------------------------------------------------------
#define POP_( NEXT ) \
	if (( stack[ NEXT ] )) { \
		i = popListItem( &stack[ NEXT ] ); \
		break; } \
	else goto RETURN;
#define POP_LAST goto RETURN;
#define POP_ITEM( stack, exponent ) { \
	exponent = popListItem( &stack ); \
	int exp = (int) exponent->ptr; \
	if ( exp & SUB ) freeItem( i ); \
	i = popListItem( &stack ); }
#define _POP( NEXT ) POP_LAST

#define POP( stack, exponent, RESUME, NEXT ) \
	for ( ; ; ) { \
		if (( i->next )) { \
			i = i->next; \
			if ( !db_private( privy, i->ptr, db ) ) { \
				e = i->ptr; \
				goto RESUME; } } \
		else if (( stack )) \
			POP_ITEM( stack, exponent ) \
		else _POP( NEXT ) }

#define POP_LIST( stack, RESUME, NEXT ) \
	for ( ; ; ) { \
		if (( i->next )) { \
			i = i->next; \
			e = i->ptr; \
			goto RESUME; } \
		else if (( stack )) \
			i = popListItem( &stack ); \
		else _POP( NEXT ) }

//---------------------------------------------------------------------------
//	FLUSH
//---------------------------------------------------------------------------
#define FLUSH( stack ) \
	while (( stack )) POP_ITEM( stack, exponent )

//---------------------------------------------------------------------------
//	assignment	- query_assignment utility
//---------------------------------------------------------------------------
static inline CNInstance * assignment( CNInstance *e, CNDB *db ) {
	// Assumption: e:( *, . ) -> returns first ( e, . )
	for ( listItem *j=e->as_sub[0]; j!=NULL; j=j->next )
		if ( !db_private( 0, j->ptr, db ) )
			return j->ptr;
	return NULL; }


#endif // QUERY_PRIVATE_H
