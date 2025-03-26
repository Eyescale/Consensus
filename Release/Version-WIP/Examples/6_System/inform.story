#define verbose
#include "include/inform.bm"

//===========================================================================
//	inform.story main narrative - subclassed from inform
//===========================================================================
: < inform

.: verify
	on : .
		do >"\nSystem Definition\n\n"
		in .%action: %(.,((.,.),?:( %((!!,DO),?), . )))
			do : action : ^action~
			do genpass
		else in .%action: %(.,((.,.),?:(.,/(ON|OFF)/)))
			do : action : ^action~
		else do : action : ~.
	else on : action : ?
		in genpass
			do >": \"%_\"\n": %?::(?,.)
			in .%generated: %!/((?,'>'),!:~'&'):%((?,DO),%?::(?,.))/
				do : generated : ^generated~
			else do : generated : '&'
		else
			do >": \"%_\" %_\n":< %?::(?,.), %?::(.,?) >
			in .%guard : %( ?, ( ., *action ))
				do : guard : ^guard~
	else on : generated : ?
		in %?: '&'
			do >"  > &\n"
		else	do >"  > \"%_\"\n": %?
		do : generated : ^generated~
	else on : generated : ~.
		in .%guard : %( ?, ((.,.), *action ))
			do : guard : ^guard~
	else on : guard : ?
		do >"  guard\n"
		in .%condition: %( ?:~(*,.), %? )
			do : condition : ^condition~
		else do : condition : ~.
	else on : condition : ?
		do >"\t\"%_\" %_\n":< %?::(?,.), %?::(.,?) >
		do : condition : ^condition~
	else on : condition : ~.
		in .%trigger: %( *guard, ( ?, *action ))
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
		else do : bar : ~.
	else on : bar : ?
		do >"\t\"%_\" %_\n":< %?::(?,.), %?::(.,?) >
		do : bar : ^bar~
	else on : bar : ~.
		do : trigger : ^trigger~
	else on : trigger : ~.
		do : guard : ^guard~
	else on : guard : ~.
		do : action : ^action~
		do >:
	else in genpass
		do ~( genpass )
		in .%action: %(.,((.,.),?:( %((!!,/(CL|SET)/),?), . )))
			do >"--\nnon-generative Actions\n\--\n\n"
			do : action : ^action~
		else do : report
	else do : report

.: report
	on : .
		in .%noop: %(?,ON):~%((!!,/(SET|DO|>)/),?)
			do : noop : ^noop~
			in ?: "init" // not counted as noop
				do : init : %?
		else do : noop : ~.
	else on : noop : ?
		in ~.:: init : %?
			do >"Warning: verify: ErrOnConditionNotActuated: \"%s\"\n": %?
		do : noop : ^noop~
	else on : noop : ~.
		in .%nocl: %(?,OFF):~%((!!,/(CL|DO|>)/),?)
			do : nocl : ^nocl~
		else do : nocl : ~.
	else on : nocl : ?
		do >&"Warning: verify: ErrOffEventNotActuated: \"%s\"\n": %?
		do : nocl : ^nocl~
	else on : nocl : ~.
		in .%onff:%(.,((.,.),(?,ON))):%(.,((.,.),(?,OFF)))
			do : onff : ^onff~
		else do : onff : ~.
	else on : onff : ?
		do >&"Warning: verify: ErrOnOffActuation: \"%s\"\n": %?
		do : onff : ^onff~
	else on : onff : ~.
		in ?: "init"
			in (((!!,DO),%?) ?: ((!!,'>'),%?))
				do >"Warning: verify: ErrInitGenerativeUsage\n"
			else in ~.: ((%?,ON), . )
				do >"Warning: verify: ErrNoInitSubscription\n"
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

