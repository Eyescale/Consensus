on init
	do (( *, phase ), INPUT )
	do (( *, state ), INPUT_SEPARATOR )

else in *phase: INPUT
	on ((*,input), . )
		in *state: INPUT_SEPARATOR
			on (( *, input ), ~%(separator,(TM,?)))
				do ( separator, (TM,*input))
			else do (( *, state ), INPUT_BLANK )
		else in *state: INPUT_BLANK
			do (( *, BLANK ), (TM,*input))
			do (( *, state ), INPUT_RIGHT )
		else in *state: INPUT_RIGHT
			do (( *, RIGHT ), (TM,*input))
			do (( *, state ), INPUT_LEFT )
		else in *state: INPUT_LEFT
			do (( *, LEFT ), (TM,*input))
			do (( *, state ), CONTINUE )
		else in *state: CONTINUE
			do (( *, state ), ( INPUT_TUPLE, INITIAL ))
		else in *state: ( INPUT_TUPLE, INITIAL )
			do (( *, INITIAL ), (TM,*input))
			do (( *, q ), (TM,*input))
			do (( *, state ), ( INPUT_TUPLE, INPUT_SYMBOL ))
		else in *state: ( INPUT_TUPLE, INPUT_SYMBOL )
			do (( *, s ), (TM,*input))
			do (( *, state ), ( INPUT_TUPLE, OUTPUT_SYMBOL ))
		else in *state: ( INPUT_TUPLE, OUTPUT_SYMBOL )
			do (( *, (SYMBOL,(*q,*s))), (TM,*input))
			do (( *, state ), ( INPUT_TUPLE, OUTPUT_MOTION ))
		else in *state: ( INPUT_TUPLE, OUTPUT_MOTION )
			do (( * , (MOTION,(*q,*s))), (TM,*input))
			do (( *, state ), ( INPUT_TUPLE, OUTPUT_STATE ))
		else in *state: ( INPUT_TUPLE, OUTPUT_STATE )
			do (( *, (STATE,(*q,*s))), (TM,*input))
			do (( *, state ), ( INPUT_TUPLE, INPUT_STATE ))
		else in *state: ( INPUT_TUPLE, INPUT_STATE )
			on (( *, input ), ~%(separator,(TM,?)))
				do (( *, q ), (TM,*input))
				do (( *, state ), ( INPUT_TUPLE, INPUT_SYMBOL ))
			else
				do (( *, p ), START )
				do (( *, state ), INPUT_TAPE )
		else in *state: INPUT_TAPE
			on (( *, input ), ~%(separator,(TM,?)))
				in (( *, input ), %(STATE,((TM,?),.)))
					do (( *, INITIAL ), (TM,*input))
					do CHECK
				else
					do (( *, p ), (*p,(TM,*input)))
					in CHECK
						do (( *, START ), (*p,(TM,*input)))
						do ~( CHECK )
			else
				do (( *, (head,state)), *INITIAL )
				do ( FINAL, *(STATE,.):~%(STATE,(?,.)))
				in ( START, . )
					do (( *, p ), %(START,.))
					do (( *, phase ), DATA_PREP )
				else
					do (( *, START ), (START,*BLANK))
					do (( *, (head,position)), (START,*BLANK))
					do (( *, phase ), RUN )
	else on : input : ~.
		do >"Error: File Format: unexpected EOF\n"
		do exit
	else do input: <

else in *phase: DATA_PREP
	on ((*,p), . )
		do (( *, (TAPE,*p)), %((.,?):*p))
		in ( *p, . )
			do (( *, (RIGHT,*p)), %(*p,.))
			do (( *, (LEFT,%(*p,.))), *p )
	else in ( *p, . )
		do (( *, p ), %(*p,.))
	else
		in ((*,START), . )
			do (( *, (head,position)), *START )
		else
			do (( *, (head,position)), %(START,.))
			do (( *, START ), %(START,.))
		do (( *, phase ), RUN )

else in *phase: RUN
	on ((*,(head,position)), . )
		in ( LEFT, *(head,position))
			do (( *, p ), *(LEFT,.):~%(LEFT,?))
		else do (( *, p ), *(head,position))
		do (( *, phase ), OUTPUT )
	else on ~((*,phase), OUTPUT  )
		in ( FINAL, *(STATE,*index))
			do exit
		else in ( TAPE, *(head,position))
			do (( *, symbol ), *(TAPE,*(head,position)))
		else do (( *, symbol ), *BLANK )
	else on ((*,symbol), . )
		in ( *(head,state), *symbol )
			do (( *, index ), (*(head,state),*symbol))
		else
			do >"Error: transition undefined\n"
			do exit
	else on ((*,index), . )
		do (( *, (TAPE,*(head,position))), *(SYMBOL,*index))
		do (( *, motion ), *(MOTION,*index))
		do (( *, (head,state)), *(STATE,*index))
	else on ((*,motion), . )
		in *motion: *RIGHT
			in ( RIGHT, *(head,position ))
				do (( *, (head,position)), *(RIGHT,*(head,position)) )
			else do (( *, (new,p)), (RIGHT,*(head,position)))
		else in *motion: *LEFT
			in ( LEFT, *(head,position ))
				do (( *, (head,position)), *(LEFT,*(head,position)) )
			else do (( *, (new,p)), (LEFT,*(head,position)))
	else on ((*,(new,p)), . )
		in *motion: *RIGHT
			do (( *, (RIGHT,*(head,position))), *(new,p))
			do (( *, (LEFT,*(new,p))), *(head,position))
		else in *motion: *LEFT
			do (( *, (LEFT,*(head,position))), *(new,p))
			do (( *, (RIGHT,*(new,p))), *(head,position))
	else do (( *, (head,position)), *(new,p))

else in *phase: OUTPUT
	on ((*,p), . )
		in *p: *START
			do >"|"
		do (( *, state ), OUTPUT_CURRENT )
	else in *state: OUTPUT_CURRENT
		in *p: *(head,position)
			do >"%s": %((.,?):*(head,state))
		else do >" "
		do (( *, state ), OUTPUT_SYMBOL )
	else in *state: OUTPUT_SYMBOL
		in ( TAPE, *p )
			do >"%s": %((.,?):*(TAPE,*p))
		else do >"%s": %((.,?):*BLANK)
		do >" "
		do ~( *, state )
	else in ( RIGHT, *p )
		do (( *, p ), *(RIGHT,*p))
	else
		do >:
		do (( *, phase ), RUN )

/* Example output:
        |.0     start from blank tape
        .0|1    moves left
        0|.1    moves right
        0|0.0   moves further right

        not shown here: *state
*/

