#ifdef TRAVERSAL_H

//===========================================================================
//	bm_traverse	- generic B% expression traversal
//===========================================================================
static BMCBTake traverse_CB( BMTraverseCB *, BMTraverseData *, \
	char **, int *, int );

static char *
bm_traverse( char *expression, BMTraverseData *traverse_data, int flags )
{
	int f_next, mode = traverse_data->done;
	traverse_data->done = 0;

	listItem **stack = traverse_data->stack;
	union { int value; void *ptr; } icast;
	char *p = expression;

	// invoke BMTermCB - if it is set - first thing
	do {
CB_TermCB	} while ( 0 );

	while ( *p && !traverse_data->done ) {
		switch ( *p ) {
			case '>':
				if is_f( EENOV ) {
CB_EEnovEndCB				f_clr( EENOV )
					f_cls }
				else if is_f( VECTOR ) {
					f_clr( VECTOR )
					f_cls }
				p++; break;
			case '<':
				if is_f( INFORMED ) // EENO
#ifdef BMTernaryOperatorCB
					f_clr( NEGATED|FILTERED|INFORMED )
#else
					{ traverse_data->done=1; break; }
#endif
				p++; break;
			case '!':
				if ( p[1]=='!' )
					p = p_prune( PRUNE_IDENTIFIER, p+2 );
				else {
CB_EMarkCharacterCB			f_cls; p++; }
				break;
			case '@':
				// Assumption: p[1]=='<' && p[2]=='\0'
CB_ActivateCB			p+=2; break;
			case '~':
				if ( p[1]=='<' ) {
CB_ActivateCB				p+=2; }
				else {
CB_NotCB				if is_f( NEGATED ) f_clr( NEGATED )	
					else f_set( NEGATED )
					p++; }
				break;
			case '{':
CB_BgnSetCB			f_push( stack )
				f_reset( FIRST|SET, 0 )
				p++;
CB_TermCB			break;
			case '}':
				if ( !is_f(SET) ) { traverse_data->done=1; break; }
				if ((*stack)) {
					icast.ptr = (*stack)->ptr;
					f_next = icast.value; }
CB_EndSetCB			f_pop( stack, 0 )
				f_cls; p++; break;
			case '|':
				if ( !is_f(LEVEL|SET) ) { traverse_data->done=1; break; }
				if ((*stack)) {
					icast.ptr = (*stack)->ptr;
					f_next = icast.value; }
CB_BgnPipeCB			f_push( stack )
				f_reset( PIPED, SET )
				p++; break;
			case '*':
				if ( !is_separator(p[1]) || strmatch("*.%(?",p[1]) ) {
CB_DereferenceCB			f_clr( INFORMED ) }
				else {
CB_StarCharacterCB			f_cls }
				p++; break;
			case '%':
				if ( p[1]=='(' ) {
CB_SubExpressionCB			p++;
					f_next = FIRST|SUB_EXPR|is_f(SET);
					if (!(mode&INFORMED) && !p_single(p))
						f_next |= COUPLE;
CB_OpenCB				f_push( stack )
					f_reset( f_next, 0 )
					p++; break; }
				else if ( p[1]=='<' ) {
CB_RegisterVariableCB			f_cls; p+=2;
					if ( strmatch( "?!(", *p ) ) {
						f_set( EENOV )
						p = p_prune( PRUNE_TERM, p )+1; }
					break; }
				else if ( strmatch( "?!|%", p[1] ) ) {
CB_RegisterVariableCB			f_cls; p+=2; break; }
				else {
CB_ModCharacterCB			f_cls; p++; break; }
				break;
			case '(':
				if ( p[1]==':' ) {
CB_LiteralCB				f_set( INFORMED )
					break; }
				else {
					f_next = FIRST|LEVEL|is_f(SET|SUB_EXPR|MARKED);
					if (!(mode&INFORMED) && !p_single(p))
						f_next |= COUPLE;
CB_OpenCB				f_push( stack )
					f_reset( f_next, 0 )
					p++;
CB_TermCB				break; }
			case ':':
				if ( p[1]=='<' ) {
					f_clr( NEGATED|FILTERED|INFORMED )
					f_set( VECTOR );
					p+=2; break; }
				else {
CB_FilterCB				f_clr( NEGATED|INFORMED )
					p++; break; }
			case ',':
				if ( !is_f(VECTOR|SET|SUB_EXPR|LEVEL) )
					{ traverse_data->done=1; break; }
CB_DecoupleCB			if is_f( SUB_EXPR|LEVEL ) f_clr( FIRST )
				f_clr( NEGATED|FILTERED|INFORMED )
				p++;
CB_TermCB			break;
			case ')':
				if ( is_f(PIPED) && !is_f(SUB_EXPR|LEVEL) ) {
					if ((*stack)) {
						icast.ptr = (*stack)->ptr;
						f_next = icast.value; }
CB_EndPipeCB				f_pop( stack, 0 ) }
				if ( !is_f(VECTOR|SET|SUB_EXPR|LEVEL) )
					{ traverse_data->done=1; break; }
				if ((*stack)) {
					icast.ptr = (*stack)->ptr;
					f_next = icast.value; }
CB_CloseCB			f_pop( stack, 0 );
				f_cls; p++; break;
			case '?':
				if is_f( INFORMED ) {
					if ( !is_f(SET|LEVEL|SUB_EXPR) ) { traverse_data->done=1; break; }
CB_TernaryOperatorCB			f_clr( NEGATED|FILTERED|INFORMED )
					f_set( TERNARY ) }
				else {
CB_WildCardCB				if ( !strncmp( p+1, ":...", 4 ) )
						p+=4;
					f_cls }
				p++; break;
			case '.':
				if ( p[1]=='(' ) {
CB_DotExpressionCB			p++;
					f_next=FIRST|DOT|LEVEL|is_f(SET|SUB_EXPR|MARKED);
					if (!(mode&INFORMED) && !p_single(p))
						f_next |= COUPLE;
CB_OpenCB				f_push( stack )
					f_reset( f_next, 0 )
					p++;
CB_TermCB				break; }
				else if ( !is_separator(p[1]) ) {
CB_DotIdentifierCB			p = p_prune( PRUNE_FILTER, p+2 );
					f_cls; break; }
				else if ( p[1]=='.' ) {
					if ( p[2]=='.' ) {
#ifdef BMEllipsisCB
CB_EllipsisCB					f_pop( stack, 0 )
#else
						if ( !strncmp( p+3, "):", 2 ) ) {
							p = p_prune( PRUNE_LIST, p );
							f_pop( stack, 0 ) }
						else {
							p+=3; }
#endif
						f_cls; break; }
					else {
CB_RegisterVariableCB				f_cls; p+=2; break; } }
				else {
CB_WildCardCB				f_cls; p++; break; }
			case '"':
CB_FormatCB			p = p_prune( PRUNE_FILTER, p );
				f_cls; break;
			case '\'':
CB_CharacterCB			p = p_prune( PRUNE_FILTER, p );
				f_cls; break;
			case '/':
CB_RegexCB			p = p_prune( PRUNE_FILTER, p );
				f_cls; break;
			default:
				if ( is_separator(*p) ) traverse_data->done = BMTraverseError;
				else {
CB_IdentifierCB				p = p_prune( PRUNE_IDENTIFIER, p );
					if ( *p=='~' && p[1]!='<' ) {
CB_SignalCB					p++; }
					f_cls; break; } } }
	switch ( traverse_data->done ) {
	case BMTraverseError:
		fprintf( stderr, ">>>>> B%%: Error: bm_traverse: syntax error "
			"in expression: %s, at %s\n", expression, p ); }
	traverse_data->flags = flags;
	traverse_data->p = p;
	return p;
}

static inline BMCBTake
traverse_CB( BMTraverseCB *cb, BMTraverseData *traverse_data, char **p, int *f_ptr, int f_next )
{
	BMCBTake take = cb( traverse_data, p, *f_ptr, f_next );
	switch ( take ) {
	case BM_CONTINUE:
	case BM_DONE:
		return take;

	case BM_PRUNE_FILTER:
		*f_ptr |= INFORMED;
		*f_ptr &= ~NEGATED;
#ifdef BMDecoupleCB
		if ( cb==BMDecoupleCB )
			*p = p_prune( PRUNE_FILTER, *p+1 );
		else
#endif
#ifdef BMFilterCB
		if ( cb==BMFilterCB )
			*p = p_prune( PRUNE_FILTER, *p+1 );
		else
#endif
		*p = p_prune( PRUNE_FILTER, *p );
		return BM_DONE;

	case BM_PRUNE_TERM:
#ifdef BMTernaryOperatorCB
		/* Special case: deternarize()
		   BMDecoupleCB is undefined in this case
		   BMFilterCB must invoke p_prune on *p
		*/
		if ( cb==BMTernaryOperatorCB ) {
			*f_ptr |= TERNARY;
			*p = p_prune( PRUNE_TERM, *p+1 ); }
#else
#ifdef BMDecoupleCB
		if ( cb==BMDecoupleCB )
			*p = p_prune( PRUNE_TERM, *p+1 );
		else
#endif
#ifdef BMFilterCB
		if ( cb==BMFilterCB )
			*p = p_prune( PRUNE_TERM, *p+1 );
		else
#endif
#endif
		*p = p_prune( PRUNE_TERM, *p );
		return BM_DONE; }
}


#endif	// TRAVERSAL_H
