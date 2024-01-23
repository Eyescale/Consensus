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
// #define TEST_PIPE
#ifdef TEST_PIPE
#define PIPE_CND	"!?("
#else
#define PIPE_CND	"!?"
#endif

typedef enum { TYPE=1, LEVEL, COUNT, PIPE, PIPE_LEVEL } StackStuff;
static inline void
stackpush_( listItem **stack, int type, int level, int count ) {
	listItem *record = new_item( type );
	record->next = (listItem *) new_pair( level, count );
	addItem( stack, record ); }
static inline int
stackpop_( listItem **stack, int *level, int *count ) {
	listItem *record = popListItem( stack );
	int type = cast_i( record->ptr );
	*level = cast_i( record->next->ptr );
	*count = cast_i( record->next->next );
	freeItem( record->next );
	freeItem( record );
	return type; }
static inline int
stacktest_( listItem *stack, int what ) {
	if (( stack )) {
		listItem *record = stack->ptr;
		switch ( what ) {
		case TYPE:  return cast_i( record->ptr );
		case LEVEL: return cast_i( record->next->ptr );
		case COUNT: return cast_i( record->next->next ); } }
	return 0; }
static inline void
retype_( listItem *stack, int type ) {
	listItem *record = stack->ptr;
	record->ptr = cast_ptr( type ); }

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
