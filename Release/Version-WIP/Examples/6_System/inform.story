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
	else do : report

#define TMP
#ifdef TMP
.: report
	per .action: %( ., ((.,.), ?:(.,/(ON|OFF)/)))
		do >": \"%_\" %_\n":< %(action:(?,.)), %(action:(.,?)) >
		in ((?:!!,DO), %(action:(?,.))) // generative action
			per ((%?,'>'),?:~'&') // generated occurrences
				do >"  > \"%_\"\n": %?
			else in ((%?,'>'),'&')
				do >"  > &\n"
		per .guard: %( ?, ((.,.), action ))
			do >"  guard\n"
			per ( ?, guard ) // guard condition
				do >"\t\"%_\" %_\n":< %?::(?,.), %?::(.,?) >
			per ( guard, ( ?, action ))
				do >"    trigger\n"
				per ( ?, %?::(?,.) ) // on event
					do >"\t\"%_\" %_\n":< %?::(?,.), %?::(.,?) >
				do >"      /\n"
				per ( ?:~!!, %?::(.,?) ) // bar event
					do >"\t\"%_\" %_\n":< %?::(?,.), %?::(.,?) >
		do >:
	do exit
#else
.: report
	on : .
		in .%action: %(.,((.,.),?:(.,/(ON|OFF))))
			do : action : ^action
	else on : action : ?
		do >": \"%_\" %_\n":< %?::(?,.), %?::(.,?) >
		in ((?:!!,DO), %?::(?,.)) // generative action
			in .%occurrence: %((%?,'>'),?:~'&'))
				do : occurrence : ^occurrence
			else do : occurrence : '&'
		else do : occurrence : ~.
	else on : occurrence :
		?
			in %?: ~'&'
				do >"  > \"%_\"\n": %?
			else	do >"  > &\n"
			in ?: ^occurrence
				do : occurrence : %?
			else do : occurrence : ~.
		~.
			in .%guard : %( ?, ( ., *action ))
				do : guard : ^guard
	else on : guard : ?
		do >"  guard\n"
		in .%condition: %( ?, %? )
			do : condition : ^condition
		else in .%trigger: %( %?, ( ?, *action ))
			do : trigger : ^trigger
	else on : condition : ?
		do >"\t\"%_\" %_\n":< %?::(?,.), %?::(.,?) >
		in ?: ^condition
			do : condition : %?
		else in .%trigger: %( *guard, ( ?, *action ))
			do : trigger : ^trigger
	else on : trigger :
		?
			do >"    trigger\n"
			in .%event: %( ?, %?::(?,.) )
				do : event : ^event
			else do : event : ~.
		~.
			in ?: ^trigger
				do : trigger : %?
			else in ?: ^guard
				do : guard : %?
			else in ?: ^action
				do : action : %?
			else
				do exit
	else on : event :
		?
			do >"\t\"%_\" %_\n":< %?::(?,.), %?::(.,?) >
			in ?: ^event
				do : event : %?
			else do : event : ~.
		~.
			do >"      /\n"
			in .%bar: %( ?:~!!, %(*trigger:(.,?)) )
				do : bar : ^bar
			else do : trigger : ~.
	else on : bar : ?
		do >"\t\"%_\" %_\n":< %?::(?,.), %?::(.,?) >
		in ?: ^bar
			do : bar : %?
		else do : trigger : ~.
#endif
