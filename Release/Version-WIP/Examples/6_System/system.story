#include "include/inform.bm"

#include "System.bm"

//===========================================================================
//	system.story main narrative - subclassed from inform
//===========================================================================
: < inform

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

