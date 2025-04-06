Objective
	we can transform any sub-expression _:%(_):_ into
			%(clip:traverse:verify)
	where
		. the overall expression is not filtered
		. clip has only . and ? terms (aka. sub)
		. traverse has only pivot, . and ? terms (sub-expr)
		. verify has everything else, i.e. *no* pivot term
Benefit
	Performances: if pivot is inside %(_?_) then exponent will start
	with SUB terms, which xp_traverse will first apply to our pivot
	then xp_verify will revert taking ALL pivot.sub.as_sub instead of
	just starting on pivot.
	
	Also locate pivot and exponent becomes a single pass exercise,
	not even requiring traversal - e.g.
	Multilevel
			%(a,(?:b,%(c,?:d)))	where c is pivot
	becomes
			%((.,(?,.)):(.,(.,%(c,?))):(a,(b,%(.,?:d))))
			  ^-- clip  ^-- traverse   ^-- verify

	with all external and internal filters inserted at proper location
	according to level

	Note
		*pivot == %((.,pivot),?):%((*,.),?) == *.:%((.,pivot),?)
	So
		%((.,pivot),?) would go into xp_traverse, and
		*. would go in the verify part

Analysis
	So we have BEFORE:expression( pivot, ? ):AFTER
					     ^------------- if there is
	Which we want to replace with:
	If there is Mark
		simplified( pivot, . ):expression( ., BEFORE:AFTER )
		       instead of pivot -----------^  ^-------------- instead of ? if there is
	Otherwise
		simplified( pivot ):expression( . ):BEFORE:AFTER
		       instead of pivot --------^
	AND passing ( expression, clip ) to verify_CB

	Examples:
	1. %(pivot,?):(A,B) becomes
		%((.,?):(pivot,.):(.,(A,B)))
	clip -----^		  ^------------ expression

	2. A:(pivot,B):C becomes
		(pivot,.):(.,B):A:C
		          ^---------------- expression
	Special cases: pivot is starred, etc.

Implementation [WIP]
	1. generate BEFORE/AFTER

		char *p, *q = expression, 
		for ( p=expression; q<pivot->p; p=q )
			q = p_prune( PRUNE_FILTER, p+1 );

		So we have pivot expression = ] p, q [

	2.a. if it's a %(_), then
		. we do m=locate_mark( p+1 )
		. we build
				] p, q [( ., ]...,p[:]q,...[ )
		replacing pivot ----------^  ^-------------- replacing mark

	2.b. if it's not a %(_) then
		. we build
				] p, q [( . ):]...,p[:]q,...[
		replacing pivot ----------^

	In any case* it starts with replacing pivot, which we got from
	bm_lookup(), which should take a &pivot_length argument,
	so that we can do - starting past %

	CNString *
	build( start, pivot_p, pivot_length, mark_p, pre, post )

		CNString *s;
		char *p=start;

		// from start, goto and replace pivot_p with '.'
		while ( p!=pivot_p ) s_put( *p++ );
		s_put( '.' )
		p += pivot_length;
=> that is: if we have mark
		// then goto and replace mark_p with [pre,start[:[post,...[
		while ( p!=mark_p ) s_put( *p++ )
		p++; // pass mark
		if ( pre || post ) {
			if (( pre )) {
				for ( char *q=pre; q!=start; ) s_put( *q++ )
				s_put( ':' ) }
			if (( post )) {
				for ( char *q=post; *q; ) s_put( *q++ ) }
		else { s_put( '.' ) }
		// finish post-mark expression
		post--; // supposedly at ':'
		while ( p!=post ) s_put( *p++ )

	*except in 2.a. if mark comes before pivot, in which case we do

		// from start, goto and replace mark_p with [pre,start[:[post,...[
		while ( p!=mark_p ) s_put( *p++ );
		if ( pre || post ) {
			if (( pre )) {
				for ( char *q=pre; q!=start; ) s_put( *q++ )
				s_put( ':' ) }
			if (( post )) {
				for ( char *q=post; *q; ) s_put( *q++ ) }
		else s_put( '.' )
		// then goto and replace pivot_p with '.'
		while ( p!=pivot_p ) s_put( *p++ )
		s_put( '.' )
		p += pivot_length;
		// finish post-pivot expression
		post--; // supposedly at ':'
		while ( p!=post ) s_put( *p++ )

	return s;

	this expression is what xp_verify() should parse for each candidate
	after clip subs application (by who?) provided by xp_traverse.

