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
	else on ( init )
		do : record : %(( init, * ), . )
		do !! Reporter (
			: head : *head @<
			: tape : *tape @<
			) ~<	// not subscribing
	else on ~< .
		do exit
	else in ( init )
		on : current : . < tape
			do ready
		else in ( ready )
			in ?: %(*record:(.,?))
				in %?: /[01]/
					do : symbol : %?
					in ( *record, . )
						do ( SIGSHIFT )~
						do ~( ready )
				else in %?: /[^ ]/
					do : state : %?
					do ( SIGSTART )~
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
	else on ~< .
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
		do : start : !! Cell @<
		do init
	else on ( init )
		do : first : *start
		do : last : *start
	else on ~< head
		do exit
	else
		on current < ?				// current set by cell
			on : value : ? < %?		// assumed concurrent
				do : value : %?
			else do : value : *BLANK
			do : current : %?		// manifest current
		else on ( left_is_current ) < current
			on ~( current ) < current
			else do : first : !! Cell (
				: right : *current @<
				) @<
		else on ( right_is_current ) < current
			on ~( current ) < current
			else do : last : !! Cell (
				: left : *current @<
				) @<
		in init
			on : symbol : ? < ..
				do : write : %?
				on ~( SIGSHIFT ) < ..	// assumed concurrent
					do : shift : RIGHT
			else on ~( SIGSTART ) < ..
				do : start : *current
			else on ~( init ) < ..
				in : current : *start
					do rollcall
				else do : reset : *start
				do ~( init )
		else
			on : current : .
				do rollcall
			else in rollcall
				on ~( SIGCALL ) < ?
					on : value : ? < %?	// assumed concurrent
						do : report : %?
					else do : report : *BLANK
					in %?: *start
						do ( SIGSTART )~
					in %?: *current
						do ( SIGCURRENT )~
					in %?: *last
						do ( SIGLAST )~
						do ~( rollcall )
			else on ~( rollcall )
				do : symbol : *value	// manifest symbol
			else on : symbol : ? < head
				do : write : %?
				on : shift : ? < head
					do : shift : %?
Cell:
	on init
		do current
	else on ~< ..
		do exit
	else on : reset : ? < ..
		in %?: %%		// reset current
			do current
			do : value : *symbol	// manifest value
		else in ( current )~
		in ( left_is_current )~
		else in ( right_is_current )~
	else on rollcall < ..			// initiate roll call
		in : left : .
		else
			do : value : *symbol	// manifest value
			do ( SIGCALL )~
	else on ~( SIGCALL ) < left		// output - roll call
		do : value : *symbol		// manifest value
		do ( SIGCALL )~
	else in current
		on : write : ? < ..
			do : symbol : %?
			on : shift : LEFT < ..		// assumed concurrent
				do left_is_current
				in : left : .
					do ~( current )
			else on : shift : RIGHT < ..
				do right_is_current
				in : right : .
					do ~( current )
		else in ( left_is_current ?: right_is_current )
			on : first : ? < ..
				do : left : %? @<
			else on : last : ? < ..
				do : right : %? @<
			do ~( current )
	else
		on left_is_current < right
			do current
			do : value : *symbol		// manifest value
			in ( right_is_current )~
		else on right_is_current < left
			do current
			do : value : *symbol		// manifest value
			in ( left_is_current )~
		else in ( left_is_current ?: right_is_current )
		else on current < left
			do left_is_current
		else on current < right
			do right_is_current

