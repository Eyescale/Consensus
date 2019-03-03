on init
	do (( *, phase ), INPUT )
	do (( *, state ), INPUT_SEPARATOR )
	do > "separator:_ "
	do input: <

else in *phase: INPUT
	in ( *, input )
	else
		do > "\nError: File Format: unexpected EOF\n"
		do exit

	in *state: INPUT_SEPARATOR
		on (( *, input ), ~%(separator,(TM,?)))
			do > "separator:_ "
			do input: <
			do ( separator, (TM,*input))
		else
			do (( *, state ), INPUT_BLANK )
			do > "blank:_ "
			do input: <
	else in *state: INPUT_BLANK
		do (( *, blank ), (TM,*input))
		do (( *, p ), START )
		do (( *, state ), INPUT_TAPE )
		do > "tape start:_ "
		do input: <
	else in *state: INPUT_TAPE
		on (( *, input ), ~%(separator,(TM,?)))
			do > ">> *p: "
			do >: *p
			do >:
			do (( *, p ), ( *p, (TM,*input)))
			do > "tape next:_ "
			do input: <
		else
			do > ">> separators: "
			do >: %(separator,(TM,?))
			do >:
			do > ">> blank: "
			do >: %((TM,?):*blank)
			do >:
			in ( START, . )
				do > ">> tape:\n"
				do (( *, p ), %(START,.))
				do (( *, phase ), OUTPUT_TAPE )
			else
				do > ">> tape: blank\n"
				do exit

else in *phase: OUTPUT_TAPE
	on (( *, p ), . )
		do > "\tsymbol:%_": %((TM,?): %((.,?): *p ))
		do > "\tleft:%_": %((TM,?): %((.,?): %((?,.):*p)))
		do > "\tright:%_": %((TM,?): %((.,?): %(*p,.)))
		do >:
	else in ( *p, . )
		do (( *, p ), %(*p,.))
	else
		do > "good bye!\n"
		do exit
