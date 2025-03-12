#include "include/inform.bm"

#include "System.bm"

//===========================================================================
//	system.story main narrative - subclassed from inform
//===========================================================================
: < inform

.: verify
	on : .
		in ?: "init"
			in (((!!,DO),%?) ?: ((!!,'>'),%?))
				do : err |: ErrInitUsage
			else in ( ?:(%?,ON), . )
				in ~.:( ., %? )
					do : init : %?
				else do : err |: ErrInitUsage
			else do : err |: ErrNoInit
		else do : err |: ErrNoInit
	else on : init : .
		in ?: "exit"
			in (((!!,DO),%?) ?: ((!!,'>'),%?))
				do : err |: ErrExitUsage
			else in ( ., ?:(%?,ON))
				in ~.:( %?, . )
					do : exit : %?
				else do : err |: ErrExitUsage
			else do : err |: ErrNoExit
		else do : err |: ErrNoExit
	else on : exit : .
		in ?:%((?,ON):~*init):~%((!!,/(SET|DO|>)/),?)
			do >&"Warning: ErrNoActuation: \"%s\" ON\n": %?
			do : err |: ErrSystemIncomplete
		else per .occ:%(?,OFF):~%((!!,/(CL|DO|>)/),?)
			in ((?:!!,SET),occ)
				do >&"Warning: recasting \"%s\"\n": occ
				do { ~(%?,SET), ((%?,DO),occ), ((%?,'>'),'&') }
	else in ?:%(?,OFF):~%((!!,/(CL|DO|>)/),?)
		do >&"Warning: ErrNoActuation: \"%s\" OFF\n": %?
		do : err |: ErrSystemIncomplete
	else do : launch

#define OCC	( %!/((?,.),!):%((.,CO),^^)/, . )
#define COS	%!/((?,CO),!):%((?,.),%?::(?,.))/
#define GEN	%!/((?,'>'),!):%((?,DO),%?::(.,(?,.)))/
.: launch
	on : .	// ascribe occurrence to cosystem
		per .occurrence: %( ?, /(ON|OFF)/ )
			in (( ?:!!, DO ), occurrence )
				do (( %?, CO ), ?::occurrence )
			else in (( !!, '>' ), occurrence )
				// done as per DO
			else do (( %?, CO ), system )
	else in (?:!!,CO):~%(?,.)
		do >&"Warning: origin not found: \"%s\"\n": %((%?,DO),?)
		do : err |: ErrSystemIncomplete
	else
		do :%((.,CO),?): !! (
			?:(*init:OCC)(%?,*^^),
			?:(*exit:~OCC)(%?,*^COS),
			?:((!!,!!),OCC){(%?)|{
				?:(GEN)(%?,{ON,OFF}),
				?:(?,%?::((?,.),.))((%?,*^COS),%|::((?,.),.)),
				?:(?:~!!,%?::((.,?),.))((%?,*^COS),%|::((.,?),.)),
				?:(?:!!,%?){(%?,%|)|
					?:(?,%?)((%?,*^COS), %|::(?,.))}}} )
		do exit

