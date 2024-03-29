:
	on init
		do : head : !! Head (
			((*,HALT), H )
			( TUPLE, {
				(A,(0,(1,(RIGHT,B))))
				(A,(1,(1,(RIGHT,H))))
				(B,(0,(0,(RIGHT,C))))
				(B,(1,(1,(RIGHT,B))))
				(C,(0,(1,(LEFT,C))))
				(C,(1,(1,(LEFT,A))))
				} )
			) @< // subscribing [default]
		do : tape : !! Tape (
			((*,BLANK), 0 )
#			(( init, * ), A )
			(( init, ... ):0 0 0 A0 0 0 0 0 0 0:)
			) @<
		do exit // just like that

: Head
	in ?: *state
		on : symbol : ? < *tape
			in %? : *HALT
				do exit
			else in ( TUPLE, (%?,(%<?>, ? )))
				do : symbol : %(%?:(?,.))
				do : shift : %(%?:(.,(?,.)))
				do : state : %(%?:(.,(.,?)))
			else
				do >"Error: head: unable to dereference ( state, symbol )\n"
				do exit
	else
		on : tape : ? < ..
			do : tape : %<?> @<
		else on : state : ? < *tape
			do : state : %<?>
		else on exit < *tape
			do exit

: Tape
	on init
		on : head : ? < .. // assumed concurrent
			do : head : %<?> @<
		do : current : (TAPE,TAPE) // instantiate origin
		do : record : %(( init, * ), . )
	else in ( init )
		in *record:(.,?)
			in %?: /[01]/
				do : *current : %?
			else in %?: ' '
				do : current : (*current,TAPE) // create right cell
			else
				do : state : %?
				do : start : *current
			do : record : ( %(*record,.) ?: ~. )
		else in ?: *start
			do : current : %?
			do ~( init )
		else
			do >"Error: tape: start not set\n"
			do exit
	else on : symbol : ? < *head
		do : *current : %<?>
		on : shift : ? < %<	// assumed concurrent
			in %<?:( RIGHT )>
				do : current : ( %(*current:(TAPE,?:~TAPE)) ?: (*current,TAPE) )
			else in %<?:( LEFT )>
				do : current : ( %(*current:(?:~TAPE,TAPE)) ?: (TAPE,*current) )
		on : state : ? < %<	// assumed concurrent
			do : state : %<?>
	else
		on : current : .
			do : address : %((TAPE,.):~%(TAPE,?)) // left-most cell
		else on : address : ?
			do >"%s%s%s ":<((%?:*start)?'|':), ((%?:*current)?*state:' '), ((*%?)?:*BLANK)>
			do : address : ( %(%?:(TAPE,?:~TAPE)) ?: %(%?,TAPE) ?: ~. ) // right cell
		else on : address : ~.
			do >: // next round
			do : symbol : ( **current ?: *BLANK )
		else on exit < *head
			do exit

