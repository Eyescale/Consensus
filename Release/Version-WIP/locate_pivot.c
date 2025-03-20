#include <stdio.h>
#include <stdlib.h>

#include "scour.h"
#include "locate_mark.h"
#include "locate_emark.h"
#include "locate_pivot.h"
#include "locate_pivot_traversal.h"

//===========================================================================
//	bm_locate_pivot
//===========================================================================
typedef struct {
	char *expression;
	unsigned int primary, secondary;
	listItem **exponent, *level, *mark_sel;
	struct { listItem *flags, *level, *base; } stack;
	} LocatePivotData;

char *
bm_locate_pivot( char *expression, listItem **xpn )
/*
	returns first term (according to prioritization in scour.h)
	which is not a wildcard and is not negated, with corresponding
	exponent (in reverse order).
*/ {
	LocatePivotData data;
	memset( &data, 0, sizeof(data) );
	data.expression = expression;
	data.primary = EENOK|EMARK|QMARK;
	data.exponent = xpn;

	BMTraverseData traversal;
	traversal.user_data = &data;
	traversal.stack = &data.stack.flags;
	traversal.done = 0;
	int primary = 1;
	for ( ; ; ) {
		char *p = locate_pivot_traversal( expression, &traversal, FIRST );
		if ( traversal.done==2 ) {
			freeListItem( &data.stack.flags );
			freeListItem( &data.stack.level );
			freeListItem( &data.stack.base );
			freeListItem( &data.mark_sel );
			return p; }
		if ( primary && data.secondary ) {
			primary = 0;
			traversal.done = 0;
			xpn_free( xpn, NULL );
			data.primary = data.secondary; }
		else {
			xpn_free( xpn, NULL );
			return NULL; } } }


//---------------------------------------------------------------------------
//	locate_pivot_traversal
//---------------------------------------------------------------------------
#define CHECK( type ) \
	(!( data->primary & type )) { \
		if ( !data->secondary || ( type < data->secondary )) \
			data->secondary = type; } \
	else

BMTraverseCBSwitch( locate_pivot_traversal )
case_( not_CB )
	_prune( BMT_PRUNE_FILTER, p+1 )
case_( bgn_selection_CB )
	listItem *mark_exp = NULL;
	bm_locate_emark( p+3, &mark_exp );
	bm_locate_mark( SUB_EXPR, p+3, &data->mark_sel );
	for ( listItem *i=data->mark_sel; i!=NULL; i=i->next )
		i->ptr = cast_ptr( cast_i(i->ptr) & 1 );
	listItem **exponent = data->exponent;
	addItem( &data->stack.base, *exponent );
	while (( mark_exp ))
		addItem( exponent, popListItem(&mark_exp) );
	addItem( &data->stack.level, data->level );
	data->level = *exponent;
	_break
case_( end_selection_CB )
	freeListItem( &data->mark_sel );
	data->level = popListItem( &data->stack.level );
	xpn_free( data->exponent, data->level );
	if ( is_f(SEL_EXPR) && !is_f(LEVEL) ) {
		listItem *base = popListItem( &data->stack.base );
		if (( base )) xpn_free( data->exponent, base ); }
	_break;
case_( filter_CB )
	xpn_free( data->exponent, data->level );
	_break
case_( comma_CB )
	xpn_free( data->exponent, data->level );
	xpn_set( *data->exponent, AS_SUB, 1 );
	_break
case_( open_CB )
	if is_f_next( ASSIGN ) {
		listItem **exponent = data->exponent;
		if CHECK( SELF ) {
			xpn_add( exponent, AS_SUB, 0 );
			_return( 2 ) }
		// apply (: operator to whatever comes next
		xpn_add( exponent, AS_SUB, 1 ); }
	else if is_f_next( COUPLE )
		xpn_add( data->exponent, AS_SUB, 0 );
	addItem( &data->stack.level, data->level );
	data->level = *data->exponent;
	_break
case_( close_CB )
	xpn_free( data->exponent, data->level );
	if is_f( ASSIGN )
		popListItem( data->exponent );
	else if is_f( COUPLE )
		popListItem( data->exponent );
	if is_f( DOT )
		popListItem( data->exponent );
	data->level = popListItem( &data->stack.level );
	if ( is_f(SUB_EXPR) && !is_f(LEVEL) ) {
		listItem *base = popListItem( &data->stack.base );
		if (( base )) xpn_free( data->exponent, base ); }
	_break;
case_( dot_identifier_CB )
	listItem **exponent = data->exponent;
	if CHECK( PERSO ) {
		xpn_add( exponent, AS_SUB, 0 );
		_return( 2 ); }
	if ( p[1]!='?' ) {
		if CHECK( IDENTIFIER ) {
			xpn_add( exponent, AS_SUB, 1 );
			(*q)++; _return( 2 ) } }
	_break
case_( identifier_CB )
	if CHECK( IDENTIFIER )
		_return( 2 )
	_break
case_( regex_CB )
	if CHECK( REGEX )
		_return( 2 )
	_break
case_( character_CB )
	if CHECK( CHARACTER )
		_return( 2 )
	_break
case_( mod_character_CB )
	if CHECK( MOD )
		_return( 2 )
	_break
case_( star_character_CB )
	if CHECK( STAR )
		_return( 2 )
	_break
case_( register_variable_CB )
	int mark = 0;
	switch ( *p ) {
	case '%':
		switch ( p[1] ) {
		case '?': mark = QMARK; break;
		case '!': mark = EMARK; break;
		case '%': mark = SELF; break;
		case '@': mark = ACTIVE; break;
		case '<': if ( p[2]=='.' ) break;
			{ mark = EENOK; break; }
		default: if ( !is_separator(p[1]) )
			{ mark = TAG; } }
		break;
	case '.': mark = PARENT; break;
	case '*': mark = EYEYE; break;
	case '^':
		switch ( p[1] ) {
		case '^': mark = EYEYE; break;
		case '.': mark = TAG; }
		break; }
	if ( mark ) {
		if CHECK( mark )
			_return( 2 ) }
	_break
case_( dereference_CB )
	listItem **exponent = data->exponent;
	if CHECK( STAR ) {
		xpn_add( exponent, SUB, 1 );
		xpn_add( exponent, AS_SUB, 0 );
		xpn_add( exponent, AS_SUB, 0 );
		_return( 2 ) }
	// apply dereferencing operator to whatever comes next
	xpn_add( exponent, SUB, 1 );
	xpn_add( exponent, AS_SUB, 0 );
	xpn_add( exponent, AS_SUB, 1 );
	_break
case_( sub_expression_CB )
	listItem *mark_exp = NULL;
	char *mark = bm_locate_mark( SUB_EXPR, p+1, &mark_exp );
	if (( mark ) && (
		!strncmp( mark, "...", 3 ) ||	// %(list,...)
		!strncmp( mark+1, ":...", 4 ) || // %(list,?:...)
		!strncmp( mark+1, ",...", 4 ) ) ) { // %((?,...):list)
			freeListItem( &mark_exp );
			if ( is_f(SEL_EXPR) && !is_f(LEVEL) ) {}
			else if CHECK( LIST_EXPR ) {
				Pair * list_expr = newPair( p, mark );
				addItem( data->exponent, list_expr );
				_return( 2 ) }
			_prune( BMT_PRUNE_FILTER, p+1 ) }
	if ( is_f(SEL_EXPR) && !is_f(LEVEL) ) {
		for ( listItem *i=data->mark_sel; i!=NULL; i=i->next )
			addItem( &mark_exp, i->ptr ); }
	listItem **exponent = data->exponent;
	addItem( &data->stack.base, *exponent );
	while (( mark_exp ))
		addItem( exponent, popListItem(&mark_exp) );
	_break
case_( dot_expression_CB )
	listItem **exponent = data->exponent;
	if CHECK( PERSO ) {
		xpn_add( exponent, AS_SUB, 0 );
		_return( 2 ) }
	// apply dot operator to whatever comes next
	xpn_add( exponent, AS_SUB, 1 );
	_break
BMTraverseCBEnd
