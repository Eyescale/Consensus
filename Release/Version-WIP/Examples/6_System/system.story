#include "include/inform.bm"

#include "cosystem/system.bm"
#include "cosystem/head.bm"
#include "cosystem/tape.bm"

//===========================================================================
//	system.story main narrative - subclassed from inform
//===========================================================================
: < inform

#define LOC	%!/((?,/(DO|CL|SET)/),!):%((?,CO),^^)/
#define COS	%!/((?,CO),!):%((?,.),%?::(?,.))/
.: carry
	on : .	// ascribe occurrence to cosystem
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
#ifdef verbose
	else on ( ., CO )
		per ?: %!/((?:!!,SET),!):%((?,CO),~system)/
			do >&"carry: \"%s\" - ascribed to '%_'\n": <
				%?, %!/((?:!!,CO),!):%((?,.),%?)/ >
		do >&"--\nSystem summary\n"
		per ?: %((.,CO), ? )
			do > "\n  %_\n": %?
			per ((?:!!,CO),%?)
				do >&"    \"%_\"\n": %((%?,/(DO|CL|SET)/),?)
			in %!/((?,'>'),!:~'&'):%((?,CO),%?)/
				do >&"  >\n"
				per ?: %!/((?,'>'),!:~'&'):%((?,CO),%?)/
					do >&"    \"%_\"\n": %?
		do > "--\n"
#endif
	else in (?:!!,CO):~%( ?, . )
		do >&"Error: origin not found: \"%s\"\n": %((%?,DO),?)
		do : err |: ErrSystemIncomplete
	else in ?: %!/((?:!!,CL),!):%((?,CO),~system)/
		do >&"Error: l%$: CL \"%s\" - not ascribable to '%_'\n": <
			%!/((?:!!,IN),!):%((?,.),%?)/, %?,
			%!/((?:!!,CO),!):%((?,.),%?)/ >
		do : err |: ErrCosystemAssignment
	else
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
