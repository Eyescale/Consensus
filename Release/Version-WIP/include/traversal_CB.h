#ifndef TRAVERSAL_CB_H
#define TRAVERSAL_CB_H

//===========================================================================
//	macros
//===========================================================================
#define BMTraverseCBSwitch( user_traversal ) \
static char *user_traversal( char *expression, BMTraverseData *traversal, int flags ) { \
	return bm_traverse( expression, traversal, flags );

#define _prune( take, p )	return ( *q=(p), take );
#define _return( val )		return ( traversal->done=(val), BMCB_DONE );
#define _continue( p )		return ( *q=(p), BMCB_DONE );
#define	_break			return BMCB_CONTINUE;

#define switch_over( cb, p_in, p_out ) { \
	int take = ( *q=p_in, cb(traversal,q,flags,f_next) ); \
	return ( take==BMCB_CONTINUE ? ( *q=p_out, take ) : take ); }

#define BMTraverseCBEnd		}

//---------------------------------------------------------------------------
//	private
//---------------------------------------------------------------------------
#define _CB( cb ) \
	if (( pruneval=cb( traversal, &p, flags, f_next ) )) {	\
		switch ( pruneval ) {				\
		case BMCB_FILTER:				\
			p = p_prune( PRUNE_FILTER, p );		\
			f_cls; break;				\
		case BMCB_TERM:					\
			p = p_prune( PRUNE_TERM, p );		\
			break;					\
		case BMCB_LEVEL:				\
			p = p_prune( PRUNE_LEVEL, p );		\
			break;					\
		case BMCB_LEVEL_DOWN:				\
			p = p_prune( PRUNE_LEVEL, p+1 );	\
			f_pop( stack, 0 ); }			\
		continue; } /* assuming BMCB_CONTINUE==0 */

//---------------------------------------------------------------------------
//	traversal callbacks
//---------------------------------------------------------------------------
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
	BMTagCB
	BMModCharacterCB
	BMStarCharacterCB
	BMEMarkCB
	BMRegisterVariableCB
	BMTernaryOperatorCB
	BMSubExpressionCB
	BMBgnSelectionCB
	BMEndSelectionCB
	BMStrExpressionCB
	BMDereferenceCB
	BMLiteralCB
	BMListCB
	BMCommaCB
	BMFilterCB
	BMPipeAssignCB
	BMWildCardCB
	BMBangBangCB
	BMDotExpressionCB
	BMDotIdentifierCB
	BMFormatCB
	BMCharacterCB
	BMRegexCB
	BMIdentifierCB
	BMSignalCB
	BMLoopCB
*/
#ifdef BMOpenCB
#define CB_OpenCB	_CB( BMOpenCB )
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

#ifdef BMTagCB
#define CB_TagCB	_CB( BMTagCB )
#else
#define CB_TagCB
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

#ifdef BMEMarkCB
#define CB_EMarkCB	_CB( BMEMarkCB )
#else
#define CB_EMarkCB
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

#ifdef BMBgnSelectionCB
#define CB_BgnSelectionCB	_CB( BMBgnSelectionCB )
#else
#define CB_BgnSelectionCB
#endif

#ifdef BMEndSelectionCB
#define CB_EndSelectionCB	_CB( BMEndSelectionCB )
#else
#define CB_EndSelectionCB
#endif

#ifdef BMStrExpressionCB
#define CB_StrExpressionCB	_CB( BMStrExpressionCB )
#else
#define CB_StrExpressionCB
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

#ifdef BMCommaCB
#define CB_CommaCB	_CB( BMCommaCB )
#else
#define CB_CommaCB
#endif

#ifdef BMFilterCB
#define CB_FilterCB	_CB( BMFilterCB )
#else
#define CB_FilterCB
#endif

#ifdef BMPipeAssignCB
#define CB_PipeAssignCB	_CB( BMPipeAssignCB )
#else
#define CB_PipeAssignCB
#endif

#ifdef BMWildCardCB
#define CB_WildCardCB	_CB( BMWildCardCB )
#else
#define CB_WildCardCB
#endif

#ifdef BMBangBangCB
#define CB_BangBangCB	_CB( BMBangBangCB )
#else
#define CB_BangBangCB
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

#ifdef BMLoopCB
#define CB_LoopCB	_CB( BMLoopCB )
#else
#define CB_LoopCB
#endif


#endif	// TRAVERSAL_CB_H
