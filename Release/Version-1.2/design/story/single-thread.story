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
		do : origin : (TAPE,TAPE) // starting cell
#		do (( init, * ), A )
		do (( init, ... ):0 0 0 A0 0 0 0 0 0 0:)
	else on ( init )
		do : current : *origin
		do : record : %(( init, * ), . )
	else in ( init )
		in ?: %(*record:(.,?))
			in %?: /[01]/
				do : *current : %?
			else in %?: ' '
				do : current : (*current,TAPE) // create RIGHT cell
			else
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
			do : address : %(((TAPE,.):~%(TAPE,?))) // left-most cell
		else on : address : ?
			in %?: *start
				do >"|"
			in %?: *current
				do >"%s": *state
			else do >" "
			do >"%s ": %( **address ?: *BLANK )
			in %?: (TAPE,.:~TAPE)
				do : address : %(%?:(.,?))
			else in ?:( %?, TAPE )
				do : address : %?
			else
				do : address : ~.
				do >:
		else on : address : ~.
			do : symbol : ( **current ?: *BLANK )
		else on : symbol : ?
			in *state : *HALT
				do exit
			else in ?: %( TUPLE, (*state,(%?, ? )))
				do : write : %(%?:(?,.))
				do : shift : %(%?:(.,(?,.)))
				do : state : %(%?:(.,(.,?)))
			else
				do >"Error: unable to dereference ( state, symbol )\n"
				do exit
		else on : write : ?
			do : *current : %?
			on : shift : RIGHT
				do : current : ( %(*current:(TAPE,?:~TAPE)) ?: (*current,TAPE) )
			else on : shift : LEFT
				do : current : ( %(*current:(?:~TAPE,TAPE)) ?: (TAPE,*current) )
		else
			do >"Error: no starting cell\n"
			do exit

