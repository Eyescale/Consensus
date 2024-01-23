#ifndef FPRINT_EXPR_H
#define FPRINT_EXPR_H
/*
   General Rules
	. carry( => level++ and RETAB
	. |{ => push level++ and count=0 and RETAB
	. |( => push level++ and -(count+1) and RETAB
	. { not following | => push count and level and output '{ '
	. } => pop count and level and
		if inside |{_}
			level-- and RETAB
		else if inside {_}
 			output ' }'
		group process subsequent '}'
	. , and count==base
		if inside |{_}
			output ',\n' and RETAB
		else if inside {_}
			output ', '
	. ) => count--
		if carry's closing ')'
			output ' )'
	. ( => count++
*/
typedef enum { TYPE=1, LEVEL, COUNT, PIPE, PIPE_LEVEL } StackStuff;
static inline void
stackpush_( listItem **stack, int type, int level, int count ) {
	listItem *record = new_item( type );
	record->next = (listItem *) new_pair( level, count );
	addItem( stack, record ); }
static inline int
stackpop_( listItem **stack, int *level, int *count ) {
	union { void *ptr; int value; } icast;
	int type;
	listItem *record = popListItem( stack );
	// inform type
	icast.ptr = record->ptr;
	type = icast.value;
	// inform level
	record = record->next;
	icast.ptr = record->ptr;
	*level = icast.value;
	// inform count
	icast.ptr = record->next;
	*count = icast.value;
	freeItem( record );
	return type; }
static inline int
stacktest_( listItem *stack, int what ) {
	union { void *ptr; int value; } icast;
	if (( stack )) {
		listItem *record = stack->ptr;
		switch ( what ) {
		case TYPE:
			icast.ptr = record->ptr;
			return icast.value;
		case LEVEL:
			record = record->next;
			icast.ptr = record->ptr;
			return icast.value;
		case COUNT:
			record = record->next;
			icast.ptr = record->next;
			return icast.value; } }
	return 0; }
static inline void
retype_( listItem *stack, int type ) {
	union { void *ptr; int value; } icast;
	listItem *record = stack->ptr;
	icast.value = PIPE_LEVEL;
	record->ptr = icast.ptr; }

#define RETAB( level ) { \
	fprintf( stream, level==ground?"\n\t":"\n" ); \
	TAB(level) }
#define test( what ) stacktest_( stack, what )
#define pop( level, count ) stackpop_( &stack, level, count )
#define push( type, level, count ) stackpush_( &stack, type, level, count )
#define retype( type ) retype_( stack, type )
#define fprint_close( stream ) \
	if ( count ) { fprintf( stream, ")" ); count--; } \
	else if ( carry ) { fprintf( stream, " )" ); carry=0; } \
	else fprintf( stream, ")" );
#define test_PIPE( count ) \
	( test(TYPE)==PIPE && test(COUNT)==(count) )


#endif	// FPRINT_EXPR_H
