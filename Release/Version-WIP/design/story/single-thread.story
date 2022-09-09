:
	on init
		do ( TUPLE, {
			(A,(0,(1,(RIGHT,B))))
			(A,(1,(1,(RIGHT,H))))
			(B,(0,(0,(RIGHT,C))))
			(B,(1,(1,(RIGHT,B))))
			(C,(0,(1,(LEFT,C))))
			(C,(1,(1,(LEFT,A))))
			} )
		do : HALT : H
		do : BLANK : 0
		do : start : (TAPE,TAPE)
#		do (( init, * ), A )
	   	do (( init, ... ):0 0 0 A0 0 0 0 0 0 0 0:)
	else on ( init )
		do : current : *start
		do : record : (( init, * ), . )
	else in ( init )
		in ?: %(*record:(.,?))
			in %?: /[01]/
				do : *current : %?
				do : current : (*current,TAPE)
			else in %?: /[^ ]/
				do : state : %?
				do : start : *current
			in ?: ( *record, . )
				do : record : %?
			else do ~( record )
		else 
			do : current : *start
			do ~( init )
	else
		on : current : .
			do : cell : (((TAPE,.):~%(TAPE,?))) // left-most cell
		else on : cell : ?
			in %?: *start
				do >"|"
			in %?: *current
				do >"%s": *state
			else do >" "
			do >"%s ": %( **cell ?: *BLANK )
			in %?: (TAPE,.:~TAPE)
				do : cell : %(%?:(.,?))
			else in ?:( %?, TAPE )
				do : cell : %?
			else
				do ~( cell )
				do >:
		else on ~( cell )
			do : symbol : ( **current ?: *BLANK )
		else on : symbol : ?
			in *state : *HALT
				do exit
			else in ?: %( TUPLE, (*state,(%?, ? )))
				do : write : %(%?:(?,.))
				do : shift : %(%?:(.,(?,.))
				do : state : %(%?:(.,(.,?))
			else
				do >"Error: unable to reference ( state, symbol )\n"
				do exit
		else on : write : ?
			do : *current : %?
			on : shift : RIGHT
				do : current : ( %(*current:(TAPE,?:~TAPE)) ?: (*current,TAPE) )
			else on : shift : LEFT
				do : current : ( %(*current:(?:~TAPE,TAPE)) ?: (TAPE,*current) )
		else
			do >"Error: no init\n"
			do exit

