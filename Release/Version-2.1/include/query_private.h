#ifndef QUERY_PRIVATE_H
#define QUERY_PRIVATE_H

typedef BMCBTake XPTraverseCB( CNInstance *, char *, BMQueryData * );
typedef enum { BM_INIT, BM_BGN, BM_END } BMVerifyOp;
#define uneq( i, operand ) ((i) && ( operand!=cast_i(i->ptr) ))

//---------------------------------------------------------------------------
//	PUSH and POP
//---------------------------------------------------------------------------
#define PUSH( stack, exponent, POP ) \
	while (( exponent )) { \
		union { void *ptr; int value; } exp; \
		exp.ptr = exponent->ptr; \
		if ( exp.value & SUB ) { \
			e = CNSUB( e, exp.value&1 ); \
			if (( e )) { \
				addItem( &stack, i ); \
				addItem( &stack, exponent ); \
				exponent = exponent->next; \
				i = newItem( e ); } \
			else goto POP; } \
		else { \
			for ( j=e->as_sub[ exp.value & 1 ]; j!=NULL; j=j->next ) \
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
	if ( cast_i(exponent->ptr) & SUB ) freeItem( i ); \
	i = popListItem( &stack ); }

#define POP_ALL( stack ) \
	while (( stack )) POP_XPi( stack, exponent )

//---------------------------------------------------------------------------
//	LUSH and LOP	- PUSH and POP list
//---------------------------------------------------------------------------
#define NEG(a)	(-(a))
/*
	in case %( list, ?:... ) we have lm==1
		we process e->as_sub[ 0 ]^n.sub[ 1 ] (n>=1)
	otherwise - case %( list, ... ) we have lm=0 then 2
		we process e->as_sub[ 0 ]^n (n>=0)
*/
#define LUSH( stack, lm, LOP ) \
	if ( lm==3 ) { \
		stack = (void*)CNSUB(e,0); } \
	else if ( lm ) { /* lm==1 or lm==2 */ \
		for ( j=e->as_sub[0]; j!=NULL; j=j->next ) \
			if ( !db_private( privy, j->ptr, db ) ) \
				break; \
		if ( !j ) { lm=NEG(lm); goto LOP; } \
		else { \
			addItem( &stack, i ); \
			addItem( &stack, j ); \
			i = j; e = i->ptr; } } \
	else { /* lm==0 */ \
		lm=2; addItem( &stack, i ); } \

#define LOP( stack, lm, LUSH ) \
	if ( lm==3 ) { \
		if (( e=(void*)stack )) goto LUSH; } \
	else if ( lm > 0 ) { /* lm==1 or lm==2 */ \
		i = popListItem( &stack ); \
		e = i->ptr; goto LUSH; } \
	else { /* lm==-1 or lm==-2 */ \
		lm = NEG(lm)&1; /* reset to resp. 1 or 0 */ \
		while (( stack )) { \
			if (( i->next )) { \
				i = i->next; \
				if ( !db_private( privy, i->ptr, db ) ) { \
					e = i->ptr; goto LUSH; } } \
			else i = popListItem( &stack ); } }

//---------------------------------------------------------------------------
//	if_LDIG and LFLUSH
//---------------------------------------------------------------------------
/*
	list_expr parity bit indicates whether stack_list_i was informed
*/
#define if_LDIG( list_expr, x, i, list_i, stack_list_i ) { \
	CNInstance *y; \
	switch ( list_expr ) { \
	case 2: case 4: ; /* %(list,...) or %(list,?:...) */ \
		if ((x) && ( y=CNSUB(x,0) )) { \
			addItem( &stack_list_i, list_i ); \
			list_i=i; j=newItem( y ); \
			list_expr++; } \
		else j = NULL; \
		break; \
	case 3: case 5: /* %(list,...) or %(list,?:...) */ \
		if ((x) && ( y=CNSUB(x,0) )) { \
			i->ptr=y; j=i; } \
		else j = NULL; \
		break; \
	case 6: case 7: /* %((?,...):list) */ \
		for ( j=x->as_sub[ 0 ]; j!=NULL; j=j->next ) \
			if ( !db_private( privy, j->ptr, db ) ) \
				break; \
		if (( j )) { \
			if ( list_expr==6 ) { \
				addItem( &stack_list_i, list_i ); \
				list_expr++; } \
			addItem( &list_i, i ); } \
		else while (( list_i )) { \
			if (( i=i->next )) { \
				if ( !db_private(privy,i->ptr,db) ) \
					{ j=i; break; } } \
			else i = popListItem( &list_i ); } \
		break; } } \
	if ( !j ) \
		LFLUSH( list_expr, i, list_i, stack_list_i ) \
	else

#define LFLUSH( list_expr, i, list_i, stack_list_i ) { \
	if ( list_expr & 1 ) { \
		switch ( list_expr-- ) { \
		case 3: case 5: /* %(list,...) or %(list,?:...) */ \
			freeItem( i ); i=list_i; \
			break; \
		case 7: /* %((?,...):list) */ \
			while (( j=popListItem(&list_i) )) i=j; \
			break; } \
		list_i = popListItem( &stack_list_i ); } }

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
