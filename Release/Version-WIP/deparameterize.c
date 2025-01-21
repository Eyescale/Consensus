#include <stdio.h>
#include <stdlib.h>

#include "deparameterize.h"
#include "deparameterize_traversal.h"

//===========================================================================
//	bm_deparameterize
//===========================================================================
typedef struct {
	listItem *list;
	struct { listItem *flags, *level; } stack;
	} DeparameterizeData;

CNString *
bm_deparameterize( char *proto ) 
/*
	The objective is to convert proto into a CNString
	where
		.identifier [not filtered] are replaced by .
		.identifier: [filtered] are removed
		(.identifier,...) are replaced by .
		(expr,...) are replaced by %(expr,...)

	For this we traverse proto generating the list
		{ [ bgn:char*, end:char* ] }
	of either
			.identifier_
		bgn ----^	   ^---- end
	   or
			  (_,...)
		bgn ------^  ^---------- end
	   or
			    (_)
		bgn --------^ ^--------- end

	which we then use to fulfill our mission
*/ {
	DeparameterizeData data;
	memset( &data, 0, sizeof(data) );
	listItem *stack = NULL;

	BMTraverseData traversal;
	traversal.user_data = &data;
	traversal.stack = &stack;
	traversal.done = INFORMED|FILTERED;

	deparameterize_traversal( proto, &traversal, FIRST );
	reorderListItem( &data.list );

	// convert sequence to CNString *
	Pair *item;
	char *p = proto;
	CNString *s = newString();
	while (( item=popListItem( &data.list ) )) {
		char *bgn = item->name;
		char *end = item->value;
		freePair( item );
		while ( p!=bgn ) StringAppend(s,*p++);
		switch ( *bgn ) {
		case '(':
			// Special case: strip (.param,...) down to .
			if ( *end=='.' && (data.list)) {
				Pair *param = data.list->ptr;
				if ( param->value==end-1 ) {
					switch ( *(char *)param->name ) {
					case '.': popListItem( &data.list );
						  freePair( param );
						  StringAppend(s,'.');
						  p = end+4; } }
				else StringAppend(s,'%'); }
			break;
		case '.':
			p = ((*end==':') ? end+1 : (StringAppend(s,'.'),end));
			break; } }
	do StringAppend(s,*p++); while ( *p );
	return s; }

//---------------------------------------------------------------------------
//	deparameterize_traversal
//---------------------------------------------------------------------------
/*
	Assumption: .identifier is followed by either one of ,:)
	Note that we do not enter negated, starred or %(...) expressions
	Note also that the caller is expected to reorder the resulting list
*/
BMTraverseCBSwitch( deparameterize_traversal )
case_( sub_expression_CB ) // provision
	_prune( BM_PRUNE_FILTER, p+1 )
case_( open_CB )
	Pair *segment = newPair( p, NULL );
	addItem( &data->stack.level, segment );
	addItem( &data->list, segment );
	_break
case_( close_CB )
	Pair *current = data->list->ptr;
	if ( !current->value ) {
		switch ( *(char *)current->name ) {
		case '.': current->value = p; } }
	Pair *segment = popListItem( &data->stack.level );
	if ( !segment->value ) segment->value = p; 
	_break
case_( comma_CB )
	Pair *current = data->list->ptr;
	if ( !current->value ) {
		switch ( *(char *)current->name ) {
		case '.': current->value = p; } }
	_break
case_( filter_CB )
	Pair *current = data->list->ptr;
	if ( !current->value ) {
		switch ( *(char *)current->name ) {
		case '.': current->value = p; } }
	_break
case_( wildcard_CB )
	if is_f( ELLIPSIS ) {
		Pair *segment = data->stack.level->ptr;
		segment->value = p; }
	_break
case_( dot_identifier_CB )
	Pair *param = newPair( p, NULL );
	addItem( &data->list, param );
	_break
BMTraverseCBEnd
