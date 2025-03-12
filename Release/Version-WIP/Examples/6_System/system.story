#include "System.bm"
#include "include/inform.bm"

// :< inform

#define UNGEN	~%((!!,'>'),?):~%!/((?,DO),!):%((?,'>'),'&')/
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
		in ?:%((?,ON):~*init):~%(.,((.,.),(?,ON))):UNGEN
			do >&"Warning: ErrNotActuated: \"%s\" ON\n": %?
			do : err |: ErrSystemIncomplete
		else in ?:%(?,OFF):~%(.,((.,.),(?,OFF))):UNGEN
			do >&"Warning: ErrNotActuated: \"%s\" OFF\n": %?
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

