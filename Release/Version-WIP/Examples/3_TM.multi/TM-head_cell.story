:
	on init
		do : head : !! Head (
			((*,HALT), H )
			((*,INIT), A )
			( TUPLE, {
				(A,(0,(1,(RIGHT,B))))
				(A,(1,(1,(RIGHT,H))))
				(B,(0,(0,(RIGHT,C))))
				(B,(1,(1,(RIGHT,B))))
				(C,(0,(1,(LEFT,C))))
				(C,(1,(1,(LEFT,A))))
				} ) )
		do : cell : !! Cell (
			((*,BLANK), 0 ) )
		do exit

: Head
	in ?: *state
		on : symbol : ? < *cell
			do > "%s": %?
			in %? : *HALT
				do exit
			else in ?: %( TUPLE, (%?,(%<?>, ? )))
				do : symbol : %(%?:(?,.))
				do : shift : %(%?:(.,(?,.)))
				do : state : %(%?:(.,(.,?)))
			else
				do >"Error: head: unable to reference ( state, symbol )\n"
				do exit
		else on : next : ? < *cell
			do *cell ~<
			do : cell : %<?> @<
	else on init
		do : state : *INIT
		on : cell : ? < ..
			do : cell : %<?> @<
	else
		do >"Error: head: no state\n"
		do exit

: Cell
	on init
		do { '|', ' ', '\n' } // for reporting
		on : head : ? < ..
			do : head : %<?>
			do first
		else in *head
			on : LEFT : %% < ..
				do : RIGHT : .. @<
			else on : RIGHT : %% < ..
				do : LEFT : .. @<
		do : current : %%
	else in : current : %%
		on : current : .
			do ( *LEFT ? signal~ : rollcall~ )
		else on ~( callout ) < *LEFT
			do rollcall~
		else on ~( rollcall )
			do > "%s": (first?'|':)
			do : symbol : (*value?:*BLANK)
			do *head @<
		else in ?: *head
			on : symbol : ? < %?
				do > "%s%s":<(*value?:*BLANK), (*RIGHT?' ':'\n')>
				do ( *RIGHT ? callout~ : ready~ )
				do : value : %<?>
				on : shift : ? < %< // assumed concurrent
					do : shift : %<?>
			else on exit < %?
				do > "%s%s":<(*value?:*BLANK), (*RIGHT?' ':'\n')>
				do ( *RIGHT ? callout~ :)
				do : current : ~.
			else in : shift : ?
				on ~( ready )
					in ~.: *%?	// invoke new cell
						do : %? : !! Cell(
							((*,BLANK), *BLANK ),
							((*,head), *head ) )
					do : current : %?
					do : shift : ~.
					do *head ~<
				else on ~( signal ) < *RIGHT
					do ready~
		else
			do >"Error: cell: no head\n"
			do exit
	else in : current : ~.
		in ?: *RIGHT
			on ~( signal ) < %?
				do exit
		else do exit
	else on : current : ?
		do : next : *%?
	else
		on ~( signal ) < *RIGHT
			do ( *LEFT ? signal~ : rollcall~ )
		else on ~( callout ) < *LEFT
			do rollcall~
		else on ~( rollcall )
			do > "%s %s%s":<(first?'|':), (*value?:*BLANK), (*RIGHT?' ':'\n')>
			do ( *RIGHT ? callout~ : signal~ )
		else on : next : %% < .
			do : current : %%
		else on exit < . // alt. { *LEFT, *RIGHT }
			do exit

