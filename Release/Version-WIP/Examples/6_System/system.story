//===========================================================================
//	Cosystem class definitions
//===========================================================================
#include "System.bm"

//===========================================================================
//	Main class definition
//===========================================================================
#define RUN
#include "inform.story"

#define OCC	( %!/((?,.),!):%((.,CO),^^)/, . )
#define COS	%!/((?,CO),!):%((?,.),%?::(?,.))/
#define GEN	%!/((?,'>'),!):%((?,DO),%?::(.,(?,.)))/

.: run
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
				do : err
			else in ( ., ?:(%?,ON))
				in ~.:( %?, . )
					do : exit : %?
				else do : err |: ErrExitUsage
			else do : err |: ErrNoExit
		else do : err |: ErrNoExit
	else on : exit : .
		per .occurrence: %( ?, /(ON|OFF)/ )
			in (( ?:!!, DO ), occurrence )
				do (( %?, CO ), ?::occurrence )
			else in (( !!, '>' ), occurrence )
				// done as per DO
			else in (( ?:!!, CL ), occurrence )
				do (( %?, CO ), system )
			else in occurrence:~%(.,((.,.),(?,.))):~%(*init:(?,.))
				do >&"Warning: origin not specified: \"%s\"\n": occurrence
				do : err |: ErrSystemIncomplete
			else do !! | {
				(( %|, SET ), occurrence ),
				(( %|, CO ), system ) }
	else per ( ?:!!, CO ):~%(?,.)
		do >&"Warning: origin not found: \"%s\"\n": %((%?,DO),?)
		do : err |: ErrSystemIncomplete
	else
		do :%((.,CO),?): !! (
			?:(*init:OCC)(%?,*^^),
			?:(*exit:~OCC)(%?,*^COS),
			?:((!!,!!),OCC){(%?)|
				?:(GEN)(%?,{ON,OFF})
				?:(?,%?::((?,.),.))((%?,*^COS),%|::((?,.),.)),
				?:(?:~!!,%?::((.,?),.))((%?,*^COS),%|::((.,?),.)),
				?:(?:!!,%?){(%?,%|)|
					?:(?,%?)((%?,*^COS), %|::(?,.))}} )
		do exit

