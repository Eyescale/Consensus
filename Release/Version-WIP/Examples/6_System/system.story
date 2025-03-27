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
				do : err |: ErrNoInitSubscription
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
		in .%onff: (.,((.,.),(.,ON)))
			do : onff : ^onff~
		else do : onff : ~.
	else on : onff : ?
		in ( %?::(?,.), ( %?::(.,(?,.)), ( %?::(.,(.,(?,.))), OFF )))
			do : err |: ErrOnOffActuation
		else do : onff : ^onff~
	else on : onff : ~.
		// ascribe occurrence to cosystem
		in ?:%((?,ON):~*init):~%((!!,/(SET|DO|>)/),?)
			do : err |: ErrOnConditionNotActuated
		else in ?:%(?,OFF):~%((!!,/(CL|DO|>)/),?)
			do : err |: ErrOffEventNotActuated
		else per .occurrence: %( ?, /(ON|OFF)/ )
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
#ifdef verbose
	else on (.,CO)
		in .%ascribed: %!/((?:!!,SET),!):%((?,CO),~system)/
			do : ascribed : ^ascribed~
		else do : ascribed : ~.
	else on : ascribed : ?
		do >&"Warning: ascribing l%$: \"%s\" to '%_'\n": <
			%!/((?:!!,IN),!):%((?,.),%?)/, %?, %!/((?:!!,CO),!):%((?,.),%?)/ >
		do : ascribed : ^ascribed~
	else on : ascribed : ~.
		do >&"--\nSystem summary\n"
		in .%cosystem: %((.,CO), ?:~system )
			do : cosystem : ^cosystem~
		else do : cosystem : system
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
		else in : cosystem : ~system
			do : cosystem : system
		else do > "--\n"
#endif
	else do : check

.: check
	on : .
		in .%noor: %(?:!!,CO):~%(?,.)
			do : noor : ^noor~
			do errout
		else do : noor : ~.
	else on : noor : ?
		do >&"Error: origin not found: \"%s\"\n": %((%?,DO),?)
		do : noor : ^noor~
	else on : noor : ~.
		in .%clor: %!/((?:!!,CL),!):%((?,CO),~system)/
			do : clor : ^clor~
			do errout
		else do : clor : ~.
	else on : clor : ?
		do >&"Error: l%$: CL \"%s\" - not ascribable to '%_'\n": <
			%!/((?:!!,IN),!):%((?,.),%?)/, %?,
			%!/((?:!!,CO),!):%((?,.),%?)/ >
		do : clor : ^clor~
	else on : clor : ~.
		in .%generated:%((?:!!,'>'),~'&')
			do : generated : ^generated~
		else do : generated : ~.
	else on : generated : ?
		in %!/((?:%generated,CO),!):%((?,'>'),%((%?,'>'),?))/:~%((%?,CO),?)
			do >&"Error: ErrMultipleOrigin: \"%_\"\n": %((%?,'>'),?)
			do errout
		do : generated : ^generated~
	else in errout
		do exit
		do >:
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

