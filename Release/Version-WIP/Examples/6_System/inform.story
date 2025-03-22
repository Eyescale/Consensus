#define verbose
#include "include/inform.bm"

//===========================================================================
//	inform.story main narrative - subclassed from inform
//===========================================================================
: < inform

.: verify
	on : .
		in .%recast: %((?:!!,SET),%(?,OFF):~%((.,CL),?))
			do : recast : ^recast~
		else do : recast : ~.
	else on : recast : ?
		do >"verify: recasting l%$: do \"%s\" as generative\n":<
			%((%?,IN),?), %((%?,SET),?) >
		do { ~(%?,SET), ((%?,DO),%((%?,SET),?)), ((%?,'>'),'&') }
		do : recast : ^recast~
	else on : recast : ~.
		do >"--\nSystem Description\n"
		in .%action: %(.,((.,.),?:(.,/(ON|OFF)/)))
			do : action : ^action~
		else do : action : ~.
	else on : action : ?
		do >"\n: \"%_\" %_\n":< %?::(?,.), %?::(.,?) >
		in ((?:!!,DO), %?::(?,.)) // generative action
			in .%generated: %((%?,'>'),?:~'&')
				do : generated : ^generated~
			else do : generated : '&'
		else do : generated : ~.
	else on : generated : ?
		in %?: '&'
			do >"  > &\n"
		else	do >"  > \"%_\"\n": %?
		do : generated : ^generated~
	else on : generated : ~.
		in .%guard : %( ?, ( ., *action ))
			do : guard : ^guard~
	else on : guard : ?
		do >"  guard\n"
		in .%condition: %( ?:~(*,.), %? )
			do : condition : ^condition~
		else in .%trigger: %( %?, ( ?, *action ))
			do : trigger : ^trigger~
	else on : condition : ?
		do >"\t\"%_\" %_\n":< %?::(?,.), %?::(.,?) >
		in ?: ^condition~
			do : condition : %?
		else in .%trigger: %( *guard, ( ?, *action ))
			do : trigger : ^trigger~
	else on : trigger : ?
		do >"    trigger\n"
		in .%event: %( ?, %?::(?,.) )
			do : event : ^event~
		else do : event : ~.
	else on : event : ?
		do >"\t\"%_\" %_\n":< %?::(?,.), %?::(.,?) >
		do : event : ^event~
	else on : event : ~.
		do >"      /\n"
		in .%bar: %( ?:~!!, %(*trigger:(.,?)) )
			do : bar : ^bar~
		else do : trigger : ^trigger~
	else on : bar : ?
		do >"\t\"%_\" %_\n":< %?::(?,.), %?::(.,?) >
		in ?: ^bar~
			do : bar : %?
		else do : trigger : ^trigger~
	else on : trigger : ~.
		in ?: ^guard~
			do : guard : %?
		else do : action : ^action~
	else on : action : ~.
		do ~(/(recast|action|generated|guard|condition|trigger|event|bar)/)
		do >:
	else do : report

.: report
	on : .
		in .%noset: %(?:~%?,ON):~%((!!,/(SET|DO|>)/),?)
			do : noset : ^noset~
			in ?: "init"
				do : init : %?
		else do : noset : ~.
	else on : noset : ?
		in ~.:: init : %?
			do >"Warning: verify: ErrNoActuation: ON \"%s\"\n": %?
		do : noset : ^noset~
	else on : noset : ~.
		in .%nocl: %(?,OFF):~%((!!,/(CL|DO|>)/),?)
			do : nocl : ^nocl~
		else do : nocl : ~.
	else on : nocl : ?
		do >&"Warning: verify: ErrNoActuation: OFF \"%s\"\n": %?
		do : nocl : ^nocl~
	else on : nocl : ~.
		in ?: "init"
			in (((!!,DO),%?) ?: ((!!,'>'),%?))
				do >"Warning: verify: ErrInitGenerativeUsage\n"
			else in ~.: ((%?,ON), . )
				do >"Warning: verify: ErrNoInitAction\n"
			else in ( ?, (%?,ON))
				do >"Warning: verify: ErrInitActuated\n"
		else do >"Warning: verify: ErrNoInit\n"
	else
		in ?: "exit"
			in (((!!,DO),%?) ?: ((!!,'>'),%?))
				do >"Warning: verify: ErrExitGenerativeUsage\n"
			else in ~.:( ., (%?,ON))
				do >"Warning: verify: ErrNoExitAction\n"
			else in ((%?,ON), . )
				do >"Warning: verify: ErrExitAsCondition\n"
		else do >"Warning: verify: ErrNoExit\n"
		do exit

