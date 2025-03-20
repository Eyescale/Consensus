#include "include/inform.bm"

//===========================================================================
//	inform.story main narrative - subclassed from inform
//===========================================================================
: < inform

.: carry
#ifdef PROPER
	on : .
		in .%action: %(.,((.,.),?:(.,/(ON|OFF))))
			do : action : ^action~
	else on : action : ?
		do >": \"%_\" %_\n":< %?::(?,.), %?::(.,?) >
		in ((?:!!,DO), %?::(?,.)) // generative action
			in .%occurrence: %((%?,'>'),?:~'&'))
				do : occurrence : ^occurrence~
			else do : occurrence : '&'
		else do : occurrence : ~.
	else on : occurrence :
		?
			in %?: ~'&'
				do >"  > \"%_\"\n": %?
			else	do >"  > &\n"
			in ?: ^occurrence~
				do : occurrence : %?
			else do : occurrence : ~.
		~.
			in .%guard : %( ?, ( ., *action ))
				do : guard : ^guard~
	else on : guard : ?
		do >"  guard\n"
		in .%condition: %( ?, %? )
			do : condition : ^condition~
		else in .%trigger: %( %?, ( ?, *action ))
			do : trigger : ^trigger~
	else on : condition : ?
		do >"\t\"%_\" %_\n":< %?::(?,.), %?::(.,?) >
		in ?: ^condition~
			do : condition : %?
		else in .%trigger: %( *guard, ( ?, *action ))
			do : trigger : ^trigger~
	else on : trigger :
		?
			do >"    trigger\n"
			in .%event: %( ?, %?::(?,.) )
				do : event : ^event~
			else do : event : ~.
		~.
			in ?: ^trigger~
				do : trigger : %?
			else in ?: ^guard~
				do : guard : %?
			else in ?: ^action~
				do : action : %?
			else
				do exit
	else on : event :
		?
			do >"\t\"%_\" %_\n":< %?::(?,.), %?::(.,?) >
			in ?: ^event~
				do : event : %?
			else do : event : ~.
		~.
			do >"      /\n"
			in .%bar: %( ?:~!!, %(*trigger:(.,?)) )
				do : bar : ^bar~
			else do : trigger : ~.
	else on : bar : ?
		do >"\t\"%_\" %_\n":< %?::(?,.), %?::(.,?) >
		in ?: ^bar~
			do : bar : %?
		else do : trigger : ~.
#else
	do >"--\nSystem Description\n\n"
	per .action: %( ., ((.,.), ?:(.,/(ON|OFF)/)))
		in ((?:!!,DO), %(action:(?,.))) // generative action
			do >": \"%_\"\n": %(action:(?,.))
			per ((%?,'>'),?:~'&') // generated occurrences
				do >"  > \"%_\"\n": %?
			else in ((%?,'>'),'&')
				do >"  > &\n"
		else do >": \"%_\" %_\n":< %(action:(?,.)), %(action:(.,?)) >
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
#endif

