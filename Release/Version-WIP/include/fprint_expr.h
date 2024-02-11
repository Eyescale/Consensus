#ifndef FPRINT_EXPR_H
#define FPRINT_EXPR_H

// #define TEST_PIPE
#ifdef TEST_PIPE
#define PIPE_CND	"!?("
#else
#define PIPE_CND	"!?"
#endif

typedef enum {
	TYPE=1,
	LEVEL,
	COUNT,
	PIPE,
	PIPE_LEVEL
	} StackStuff;

#define RETAB( level ) { \
	fprintf( stream, level==ground?"\n\t":"\n" ); \
	TAB(level) }
#define test( what ) stacktest_( stack, what )
#define test_PIPE( count ) \
	( test(TYPE)==PIPE && test(COUNT)==(count) )
#define pop( level, count ) stackpop_( &stack, level, count )
#define push( type, level, count ) stackpush_( &stack, type, level, count )
#define retype( type ) retype_( stack, type )
#define fprint_close( stream ) \
	if ( count ) { putc( ')', stream ); count--; } \
	else if ( carry ) { fprintf( stream, " )" ); carry=0; } \
	else putc( ')', stream );

static inline void stackpush_( listItem **stack, int type, int level, int count ) {
	listItem *record = new_item( type );
	record->next = (listItem *) new_pair( level, count );
	addItem( stack, record ); }
static inline int stackpop_( listItem **stack, int *level, int *count ) {
	listItem *record = popListItem( stack );
	int type = cast_i( record->ptr );
	*level = cast_i( record->next->ptr );
	*count = cast_i( record->next->next );
	freeItem( record->next );
	freeItem( record );
	return type; }
static inline int stacktest_( listItem *stack, int what ) {
	if (( stack )) {
		listItem *record = stack->ptr;
		switch ( what ) {
		case TYPE:  return cast_i( record->ptr );
		case LEVEL: return cast_i( record->next->ptr );
		case COUNT: return cast_i( record->next->next ); } }
	return 0; }
static inline void retype_( listItem *stack, int type ) {
	listItem *record = stack->ptr;
	record->ptr = cast_ptr( type ); }


#endif	// FPRINT_EXPR_H
