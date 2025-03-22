#include "include/inform.bm"

#include "cosystem/system.bm"
#include "cosystem/head.bm"
#include "cosystem/tape.bm"

//===========================================================================
//	system.story main narrative - subclassed from inform
//===========================================================================
: < inform

.: verify
	on : .
		in ?: "init"
			in (((!!,DO),%?) ?: ((!!,'>'),%?))
				do : err |: ErrInitGenerativeUsage
			else in ~.: ((%?,ON), . )
				do : err |: ErrNoInitAction
			else in ( ., (%?,ON))
				do : err |: ErrInitActuated
			else do : init : (%?,ON)
		else do : err |: ErrNoInit
	else on : init : .
		in ?: "exit"
			in (((!!,DO),%?) ?: ((!!,'>'),%?))
				do : err |: ErrExitGenerativeUsage
			else in ~.:( ., (%?,ON))
				do : err |: ErrNoExitAction
			else in ((%?,ON), . )
				do : err |: ErrExitAsCondition
			else do : exit : (%?,ON)
		else do : err |: ErrNoExit
	else on : exit : .
		in .%recast: %((?:!!,SET),%(?,OFF):~%((.,CL),?))
			do : recast : ^recast~
		else do : recast : ~.
	else on : recast : ?
#ifdef verbose
		do >&"verify: recasting l%$: do \"%s\" as generative\n":<
			%((%?,IN),?), %((%?,SET),?) >
#endif
		do { ~(%?,SET), ((%?,DO),%((%?,SET),?)), ((%?,'>'),'&') }
		do : recast : ^recast~
	else on : recast : ~.
		// ascribe occurrence to cosystem
		per .occurrence: %( ?, /(ON|OFF)/ )
			in (( ?:!!, DO ), occurrence )
				do (( %?, CO ), ?::occurrence )
			else in (( !!, '>' ), occurrence )
				// done as per DO
			else in (( ?:!!, /(CL|SET)/ ), occurrence )
				do (( %?, CO ), ?::occurrence::system )
			else in : init : ( occurrence, . ) // not actuated
				do !! | {
					((%|,SET), occurrence ),
					((%|,CO), system ) }
	else do : checklist

//---------------------------------------------------------------------------
//	checklist
//---------------------------------------------------------------------------
.: checklist
	on : .
#ifdef verbose
		in .%ascribed: %!/((?:!!,SET),!):%((?,CO),~system)/
			do : ascribed : ^ascribed~
		else do : ascribed : ~.
	else on : ascribed : ?
		do >&"Warning: \"%s\" - ascribed to '%_'\n": <
			%?, %!/((?:!!,CO),!):%((?,.),%?)/ >
		do : ascribed : ^ascribed~
	else on : ascribed : ~.
		do >&"--\nSystem summary\n"
		in .%cosystem: %((.,CO), ? )
			do : cosystem : ^cosystem~
		else do : cosystem : ~.
	else on : cosystem : ?
		do > "\n  %_\n": %?
		in .%action: %!/((?,/(DO|CL|SET)/),!):%((?,CO),%?)/
			do : action : ^action~
	else on : action : ?
		do >&"    \"%_\"\n": %?
		do : action : ^action~
	else on : action : ~.
		in .%generated: %!/((?,'>'),!:~'&'):%((?,CO),*cosystem)/
			do : generated : ^generated~
			do >&"  >\n"
		else do : generated : ~.
	else on : generated : ?
		do >&"    \"%_\"\n": %?
		do : generated : ^generated~
	else on : generated : ~.
		in ?: ^cosystem~
			do : cosystem : %?
		else do > "--\n"
#endif
	else in ?:%((?,ON):~*init):~%((!!,/(SET|DO|>)/),?)
		do >&"Error: carry: ErrNoActuation: ON \"%s\"\n": %?
		do exit
	else in ?:%(?,OFF):~%((!!,/(CL|DO|>)/),?)
		do >&"Error: carry: ErrNoActuation: OFF \"%s\"\n": %?
		do exit
	else in (?:!!,CO):~%( ?, . )
		do >&"Error: carry: origin not found: \"%s\"\n": %((%?,DO),?)
		do exit
	else in ?: %!/((?:!!,CL),!):%((?,CO),~system)/
		do >&"Error: l%$: CL \"%s\" - not ascribable to '%_'\n": <
			%!/((?:!!,IN),!):%((?,.),%?)/, %?,
			%!/((?:!!,CO),!):%((?,.),%?)/ >
		do exit
	else
		do : launch

//---------------------------------------------------------------------------
//	launch
//---------------------------------------------------------------------------
#define LOC	%!/((?,/(DO|CL|SET)/),!):%((?,CO),^^)/
#define COS	%!/((?,CO),!):%((?,.),%?::(?,.))/
.: launch
	do : %((.,CO),?) : !! (
		?:(*init:(LOC,.))(%?,*^^)
		?:(*exit:~(LOC,.))(%?,*^COS),
		?:(%!/((?,'>'),!:~'&'):%((?,CO),^^)/)(%?)
		?:((!!,!!),(LOC,.)){(%?)|{
			?:(?,%?::((?,.),.))((%?,*^COS),%|::((?,.),.)),
			?:(?:~!!,%?::((.,?),.))((%?,*^COS),%|::((.,?),.)),
			?:(?:!!,%?){(%?,%|)|
				?:(?,%?)((%?,*^COS),%|::(?,.))}}} )
	do exit
