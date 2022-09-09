:
	on init
		do : tape : !! Tape ( : BLANK : 0 ) @<
		do : head : !! Head (
			: HALT : H
			( TUPLE, {
				(A,(0,(1,(RIGHT,B))))
				(A,(1,(1,(RIGHT,H))))
				(B,(0,(0,(RIGHT,C))))
				(B,(1,(1,(RIGHT,B))))
				(C,(0,(1,(LEFT,C))))
				(C,(1,(1,(LEFT,A))))
				} )
			) @<	// subscribing
#		do (( init, * ), A )
		do (( init, ... ):0 0 0 A0 0 0 0 0 0 0 0:)
	else on ~< .
		do exit
	else on ( init )
		do : record : %(( init, * ), . )
		do !! Reporter (
			: head : *head @<
			: tape : *tape @<
			) ~<	// not subscribing
	else in ( init )
		on : current : . < tape
			do ready
		else in ( ready )
			in ?: %(*record:(.,?))
				in %?: /[01]/
					do : symbol : %?
					in ( *record, . )
						do SIGSHIFT~
						do ~( ready )
				else in %?: /[^ ]/
					do : state : %?
					do SIGSTART~
				in ?: ( *record, . )
					do : record : %?
				else do ~( record )
			else
				do ~( init )
	else do exit

Reporter:
	on init
		do ~< ..
	else on : state : ? < head
		do : state : %?
	else on : report : ? < tape
		on ~( SIGSTART ) < tape
			do >"|"
		on ~( SIGCURRENT ) < tape
			do >"%s": *state
		else do >" "
		do >"%s": %?
		on ~( SIGLAST ) < tape
			do >"\n"
		else do >" "
	else on ~< head
		do exit
Head:
	on init
		on : tape : ? < ..	// assumed concurrent
			do : tape : %? @<
	else on : state : ? < ..
		do : state : %?
	else on : symbol : ? < tape
		in : state : *HALT
			do exit
		else in ?: %( TUPLE, (*state,(%?, ? )))
			do : symbol : %(%?:(?,.))
			do : shift : %(%?:(.,(?,.)))
			do : state : %(%?:(.,(.,?)))
		else
			do >"Error: head: unable to reference ( state, symbol )\n"
			do exit
Tape:
	on init
		on : head : ? < ..	// assumed concurrent
			do : head : %? @<
		do : start : (TAPE,TAPE)
		do init
	else on ~< head
		do exit
	else on ( init )
		do : first : *start
		do : last : *start
		do : current : *start
	else in init
		on : symbol : ? < ..
			do : *current : %?
			on ~( SIGSHIFT ) < ..	// assumed concurrent
				do : current : (*current,TAPE)
		else on ~( SIGSTART ) < ..
			do : start : *current
		else on ~( init ) < ..
			do : current : *start
			do ~( init )
	else
		on : current : .
			do : cell : (((TAPE,.):~%(TAPE,?))) // left-most cell
		else on : cell : ?
			do : report : %( *%? ?: *BLANK )
			in %?: *start
				do SIGSTART~
			in %?: *current
				do SIGCURRENT~
			in %?: (TAPE,.:~TAPE)
				do : cell : %(%?:(.,?))
			else in ?:( %?, TAPE )
				do : cell : %?
			else
				do SIGLAST~
				do ~( cell )
		else on ~( cell )
			do : symbol : **current	// manifest symbol
		else on : symbol : ? < head
			do : *current : %?
			on : shift : RIGHT < head
				do : current : ( %(*current:(TAPE,?:~TAPE)) ?: (*current,TAPE) )
			else on : shift : LEFT < head
				do : current : ( %(*current:(?:~TAPE,TAPE)) ?: (TAPE,*current) )

