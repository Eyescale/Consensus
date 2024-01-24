#ifndef INSTANTIATE_PRIVATE_H
#define INSTANTIATE_PRIVATE_H

typedef struct {
	struct { listItem *stack; int current; } newborn;
	struct { listItem *flags; } stack;
	listItem *sub[ 2 ];
	listItem *results;
	BMContext *ctx, *carry;
	CNDB *db;
} InstantiateData;

#define NDX ( is_f( FIRST ) ? 0 : 1 )

static inline int newborn_authorized( InstantiateData *data ) {
	int current = data->newborn.current;
	if ( current ) return ( current > 0 );
	for ( listItem *i=data->newborn.stack; i!=NULL; i=i->next )
		if (( current=cast_i( i->ptr ) ))
			return ( current > 0 );
	return 0; }

static inline listItem * subx( char *p ) {
	if ( *p=='.' ) return NULL;
	listItem *sub = NULL;
	for ( int ndx=0; ; )
		switch ( *p++ ) {
		case '(': add_item( &sub, ndx ); ndx=0; break;
		case ',': sub->ptr=cast_ptr( 1 ); break;
		case '?': reorderListItem( &sub ); return sub; } }

static inline CNInstance * xsub( CNInstance *x, listItem *xpn ) {
	if ( !xpn ) return x;
	for ( listItem *i=xpn; i!=NULL; i=i->next ) {
		union { void *ptr; int value; } exp;
		exp.ptr = i->ptr;
		x = CNSUB( x, exp.value );
		if ( !x ) return NULL; }
	return x; }

#define ifn_instantiate_traversal( expr, extra ) \
	traverse_data->done = INFORMED|LITERAL; \
	p = instantiate_traversal( expr, traverse_data, FIRST|extra ); \
	if ( traverse_data->done==2 || !data->sub[ 0 ] )
#define BM_ASSIGN( sub, db ) \
	for ( listItem *i=sub[0], *j=sub[1]; (i)&&(j); i=i->next, j=j->next ) \
		db_assign( i->ptr, j->ptr, db ); \
	freeListItem( &sub[ 0 ] ); \
	freeListItem( &sub[ 1 ] );
#define BM_UNASSIGN( sub, db ) \
	for (CNInstance*e;(e=popListItem( &sub[0] ));) \
		db_unassign( e, db );

#define CONNECTED( e )	((e->as_sub[0])||(e->as_sub[1]))


#endif	// INSTANTIATE_PRIVATE_H
