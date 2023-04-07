#ifndef QUERY_PRIVATE_H
#define QUERY_PRIVATE_H

//---------------------------------------------------------------------------
//	PUSH and POP
//---------------------------------------------------------------------------
#define PUSH( stack, exponent, FAIL ) \
	while (( exponent )) { \
		int exp = (int) exponent->ptr; \
		if ( exp & SUB ) { \
			e = CNSUB( e, exp&1 ); \
			if (( e )) { \
				addItem( &stack, i ); \
				addItem( &stack, exponent ); \
				exponent = exponent->next; \
				i = newItem( e ); } \
			else goto FAIL; } \
		else { \
			for ( j=e->as_sub[ exp & 1 ]; j!=NULL; j=j->next ) \
				if ( !db_private( privy, j->ptr, db ) ) \
					break; \
			if (( j )) { \
				addItem( &stack, i ); \
				addItem( &stack, exponent ); \
				exponent = exponent->next; \
				i = j; e = j->ptr; } \
			else goto FAIL; } }

#define POP( stack, exponent, PASS, NEXT ) \
	for ( ; ; ) { \
		if (( i->next )) { \
			i = i->next; \
			if ( !db_private( privy, i->ptr, db ) ) { \
				e = i->ptr; \
				goto PASS; } } \
		else if (( stack )) \
			POP_XPi( stack, exponent ) \
		else break; } \
	if ( !NEXT ) break;

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
#define LUSH( stack, FAIL ) \
	addItem( &stack, i ); \
	for ( i=e->as_sub[0]; i!=NULL; i=i->next ) \
		if ( !db_private( privy, i->ptr, db ) ) \
			break; \
	if (( i )) { \
		addItem( &stack, i ); \
		e = ((CNInstance *) i->ptr )->sub[ 1 ]; } \
	else { \
		i = popListItem( &stack ); \
		goto FAIL; }

#define LOP( stack, PASS, NEXT ) \
	for ( ; ; ) { \
		if (( i->next )) { \
			i = i->next; \
			if ( !db_private( privy, i->ptr, db ) ) { \
				e = i->ptr; \
				goto PASS; } } \
		else if (( stack )) \
			i = popListItem( &stack ); \
		else break; } \
	if ( !NEXT ) break;

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
