#ifndef TRAVERSAL_CB_H
#define TRAVERSAL_CB_H

//===========================================================================
//	macros
//===========================================================================
#define BMTraverseCBSwitch( user_traversal ) \
static char *user_traversal( char *expression, BMTraverseData *traverse_data, int flags ) { \
	return bm_traverse( expression, traverse_data, flags );

#define _prune( take, p )	return ( *q=(p), take );
#define _return( val )		return ( traverse_data->done=(val), BM_DONE );
#define _continue( p )		return ( *q=(p), BM_DONE );
#define	_break			return BM_CONTINUE;
#define BMTraverseCBEnd		}

#define _CB( cb ) \
	switch ( cb(traverse_data,&p,flags,f_next) ) {	\
	case BM_DONE: continue;				\
	case BM_CONTINUE: break;			\
	case BM_PRUNE_FILTER:				\
		f_cls					\
		p = p_prune( PRUNE_FILTER, p );		\
		continue;				\
	case BM_PRUNE_TERM:				\
		p = p_prune( PRUNE_TERM, p );		\
		continue;				\
	case BM_PRUNE_LEVEL:				\
		p = prune_level( p );			\
		continue; }

//===========================================================================
//	traversal callbacks
//===========================================================================
/*
	BMOpenCB
	BMCloseCB
	BMEEnovEndCB
	BMTermCB
	BMActivateCB
	BMNotCB
	BMBgnSetCB
	BMEndSetCB
	BMBgnPipeCB
	BMEndPipeCB
	BMModCharacterCB
	BMStarCharacterCB
	BMEMarkCharacterCB
	BMRegisterVariableCB
	BMTernaryOperatorCB
	BMSubExpressionCB
	BMDereferenceCB
	BMLiteralCB
	BMListCB
	BMDecoupleCB
	BMFilterCB
	BMWildCardCB
	BMDotExpressionCB
	BMDotIdentifierCB
	BMFormatCB
	BMCharacterCB
	BMRegexCB
	BMIdentifierCB
	BMSignalCB
	BMNewBornCB
	BMLoopCB
*/
#ifdef BMOpenCB
#define CB_OpenCB	if ( !(mode&TERNARY) || p_ternary(p) ) \
				_CB( BMOpenCB )
#else
#define CB_OpenCB
#endif

#ifdef BMCloseCB
#define CB_CloseCB	_CB( BMCloseCB )
#else
#define CB_CloseCB
#endif

#ifdef BMEEnovEndCB
#define CB_EEnovEndCB	_CB( BMEEnovEndCB )
#else
#define CB_EEnovEndCB
#endif

#ifdef BMTermCB
#define CB_TermCB	_CB( BMTermCB )
#else
#define CB_TermCB
#endif

#ifdef BMActivateCB
#define CB_ActivateCB	_CB( BMActivateCB )
#else
#define CB_ActivateCB
#endif

#ifdef BMNotCB
#define CB_NotCB	_CB( BMNotCB )
#else
#define CB_NotCB
#endif

#ifdef BMBgnSetCB
#define CB_BgnSetCB	_CB( BMBgnSetCB )
#else
#define CB_BgnSetCB
#endif

#ifdef BMEndSetCB
#define CB_EndSetCB	_CB( BMEndSetCB )
#else
#define CB_EndSetCB
#endif

#ifdef BMBgnPipeCB
#define CB_BgnPipeCB	_CB( BMBgnPipeCB )
#else
#define CB_BgnPipeCB
#endif

#ifdef BMEndPipeCB
#define CB_EndPipeCB	_CB( BMEndPipeCB )
#else
#define CB_EndPipeCB
#endif

#ifdef BMModCharacterCB
#define CB_ModCharacterCB	_CB( BMModCharacterCB )
#else
#define CB_ModCharacterCB
#endif

#ifdef BMStarCharacterCB
#define CB_StarCharacterCB	_CB( BMStarCharacterCB )
#else
#define CB_StarCharacterCB
#endif

#ifdef BMEMarkCharacterCB
#define CB_EMarkCharacterCB	_CB( BMEMarkCharacterCB )
#else
#define CB_EMarkCharacterCB
#endif

#ifdef BMRegisterVariableCB
#define CB_RegisterVariableCB	_CB( BMRegisterVariableCB )
#else
#define CB_RegisterVariableCB
#endif

#ifdef BMTernaryOperatorCB
#define CB_TernaryOperatorCB	_CB( BMTernaryOperatorCB )
#else
#define CB_TernaryOperatorCB
#endif

#ifdef BMSubExpressionCB
#define CB_SubExpressionCB	_CB( BMSubExpressionCB )
#else
#define CB_SubExpressionCB
#endif

#ifdef BMDereferenceCB
#define CB_DereferenceCB	_CB( BMDereferenceCB )
#else
#define CB_DereferenceCB
#endif

#ifdef BMLiteralCB
#define CB_LiteralCB	_CB( BMLiteralCB )
#else
#define CB_LiteralCB	p = p_prune( PRUNE_LITERAL, p );
#endif

#ifdef BMListCB
#define CB_ListCB	_CB( BMListCB )
#else
#define CB_ListCB	p = p_prune( PRUNE_LIST, p );
#endif

#ifdef BMDecoupleCB
#define CB_DecoupleCB	_CB( BMDecoupleCB )
#else
#define CB_DecoupleCB
#endif

#ifdef BMFilterCB
#define CB_FilterCB	_CB( BMFilterCB )
#else
#define CB_FilterCB
#endif

#ifdef BMWildCardCB
#define CB_WildCardCB	_CB( BMWildCardCB )
#else
#define CB_WildCardCB
#endif

#ifdef BMDotExpressionCB
#define CB_DotExpressionCB	_CB( BMDotExpressionCB )
#else
#define CB_DotExpressionCB
#endif

#ifdef BMDotIdentifierCB
#define CB_DotIdentifierCB	_CB( BMDotIdentifierCB )
#else
#define CB_DotIdentifierCB
#endif

#ifdef BMFormatCB
#define CB_FormatCB	_CB( BMFormatCB )
#else
#define CB_FormatCB
#endif

#ifdef BMCharacterCB
#define CB_CharacterCB	_CB( BMCharacterCB )
#else
#define CB_CharacterCB
#endif

#ifdef BMRegexCB
#define CB_RegexCB	_CB( BMRegexCB )
#else
#define CB_RegexCB
#endif

#ifdef BMIdentifierCB
#define CB_IdentifierCB	_CB( BMIdentifierCB )
#else
#define CB_IdentifierCB
#endif

#ifdef BMSignalCB
#define CB_SignalCB	_CB( BMSignalCB )
#else
#define CB_SignalCB
#endif

#ifdef BMNewBornCB
#define CB_NewBornCB	_CB( BMNewBornCB )
#else
#define CB_NewBornCB
#endif

#ifdef BMLoopCB
#define CB_LoopCB	_CB( BMLoopCB )
#else
#define CB_LoopCB
#endif


#endif	// TRAVERSAL_CB_H
