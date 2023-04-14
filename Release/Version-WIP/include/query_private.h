#ifndef QUERY_PRIVATE_H
#define QUERY_PRIVATE_H

//---------------------------------------------------------------------------
//	PUSH and POP
//---------------------------------------------------------------------------
#define PUSH( stack, exponent, POP ) \
	while (( exponent )) { \
		int exp = (int) exponent->ptr; \
		if ( exp & SUB ) { \
			e = CNSUB( e, exp&1 ); \
			if (( e )) { \
				addItem( &stack, i ); \
				addItem( &stack, exponent ); \
				exponent = exponent->next; \
				i = newItem( e ); } \
			else goto POP; } \
		else { \
			for ( j=e->as_sub[ exp & 1 ]; j!=NULL; j=j->next ) \
				if ( !db_private( privy, j->ptr, db ) ) \
					break; \
			if (( j )) { \
				addItem( &stack, i ); \
				addItem( &stack, exponent ); \
				exponent = exponent->next; \
				i = j; e = j->ptr; } \
			else goto POP; } }

#define POP( stack, exponent, PUSH ) \
	for ( ; ; ) { \
		if (( i->next )) { \
			i = i->next; \
			if ( !db_private( privy, i->ptr, db ) ) { \
				e = i->ptr; \
				goto PUSH; } } \
		else if (( stack )) \
			POP_XPi( stack, exponent ) \
		else break; }

#define POP_XPi( stack, exponent ) { \
	exponent = popListItem( &stack ); \
	int exp = (int) exponent->ptr; \
	if ( exp & SUB ) freeItem( i ); \
	i = popListItem( &stack ); }

#define POP_ALL( stack ) \
	while (( stack )) POP_XPi( stack, exponent )

//---------------------------------------------------------------------------
//	LUSH and LOP	- PUSH and POP list
//---------------------------------------------------------------------------
/*
	in case %( list, ?:... ) we have lm==1
		we process e->as_sub[ 0 ]^n.sub[ 1 ] (n>=1)
	otherwise - case %( list, ... )
		we process e->as_sub[ 0 ]^n (n>=0)
*/
#define LUSH( stack, lm, LOP ) \
	addItem( &stack, i ); \
	if ( lm ) { \
		for ( i=e->as_sub[0]; i!=NULL; i=i->next ) \
			if ( !db_private( privy, i->ptr, db ) ) \
				break; \
		if (( i )) { \
			addItem( &stack, i ); \
			e = i->ptr; \
			if ( lm & 1 ) { \
				e = e->sub[ 1 ]; } } \
		else { \
			i = popListItem( &stack ); \
			goto LOP; } }

#define LOP( stack, lm, LUSH, NEXT ) \
	for ( ; ; ) { \
		if (( i->next )) { \
			i = i->next; \
			if ( !db_private( privy, i->ptr, db ) ) { \
				e = i->ptr; lm &= 1; \
				goto LUSH; } } \
		else if (( stack )) \
			i = popListItem( &stack ); \
		else { lm &= 1; break; } } \
	if ( !NEXT ) break;

//---------------------------------------------------------------------------
//	LFLUSH
//---------------------------------------------------------------------------
#define	LFLUSH( list_exp, i, list_i, stack ) { \
	if ( list_exp & 1 ) { \
		switch ( list_exp-- ) { \
		case 7: while (( j=popListItem(&list_i) )) i=j; break; \
		default: freeItem( i ); i = list_i; } \
		list_i = popListItem( &stack ); } }

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
