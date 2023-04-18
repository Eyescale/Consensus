#include <stdio.h>
#include <stdlib.h>

#include "traverse.h"
#include "deparameterize.h"

#if 0
CNString *
bm_deparameterize( char *proto )
{
	int ellipsis = 0;
	CNString *s = newString();
	for ( char *p=proto; *p; p++ ) {
		// reducing proto params as we go
		if ( !strncmp( p, "...", 3 ) ) {
			ellipsis = 1;
			s_add( "..." );
			p += 3; }
		else if ( *p=='.' ) {
			do p++; while ( !is_separator(*p) );
			if ( *p==':' ) continue;
			else StringAppend( s, '.' ); }
		StringAppend( s, *p ); }
	if ( ellipsis )
		StringAffix( s, '%' );
	return s;
}
#else
//===========================================================================
//	bm_deparameterize
//===========================================================================
#include "deparameterize_traversal.h"

typedef struct {
	listItem *list;
	struct { listItem *flags, *level; } stack;
} DeparameterizeData;

CNString *
bm_deparameterize( char *proto ) 
/*
	The objective is to convert proto into a CNString
	where
		.identifier (not filtered) are replaced by .
		.identifier: (filtered) are removed
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
	Note that (.param,...) will be replaced by .
*/
{
	DeparameterizeData data;
	memset( &data, 0, sizeof(data) );
	listItem *stack = NULL;

	BMTraverseData traverse_data;
	traverse_data.user_data = &data;
	traverse_data.stack = &stack;
	traverse_data.done = INFORMED|FILTERED;

	deparameterize_traversal( proto, &traverse_data, FIRST );
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
						  StringAppend(s,'.');
						  p = end+4; } }
				else StringAppend(s,'%'); }
			break;
		case '.':
			p = ((*end==':') ? end+1 : (StringAppend(s,'.'),end));
			break; } }
	do StringAppend(s,*p++); while ( *p );

#if 0
	p = StringFinish(s,0);
	fprintf( stderr, "deparameterized: %s\n", p );
	freeString( s );
	exit( 0 );
#endif
	return s;
}

//---------------------------------------------------------------------------
//	deparameterize_traversal
//---------------------------------------------------------------------------
/*
	Assumption: .identifier is followed by either one of ,:)
*/
BMTraverseCBSwitch( deparameterize_traversal )
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
case_( decouple_CB )
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

#endif
