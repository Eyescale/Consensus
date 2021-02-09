
: // base narrative
	.carry
	on init
		do ( Schema, (h,(e,(l,(l,(o,(',',(' ',(w,(o,(r,(l,(d,'\0')))))))))))) )
		do ( *, input )	// required to catch EOF first frame
		do ((*,record), (record,*))
		do INPUT
	else in INPUT
		%( schema, . )
		on INPUT
			do ( schema, %( Schema, ? ) )
		else in ( schema, . )
			in ( schema, . ): %( ?, (COMPLETE,.))
				do ~( INPUT )
			else in ( schema, . ): ~%( ?, READY )
			else // all schema ready
				on ( ., READY )
					in *carry
						do ((*,input), *carry )
						do ~( *, carry )
					else do input:"%c"<
				else on ((*,input), . )
					do ((*,record), (*record,*input))
				else on ~( *, input )
					do ((*,record), (*record,EOF))
		else // all schemas failed
			do ~( INPUT )
	else on ~( INPUT )
		in (*,input): ~%(?,.) // nop
			do exit
		else in ( schema, . ): %( ?, (COMPLETE,']'))
		else in *record: ~((record,*), . ) // not first input
			do ((*,carry), %((.,?):*record))
			do ((*,record), %((?,.):*record))
		do OUTPUT
	else in OUTPUT
		.f
		on OUTPUT
			do ((*,f), %((record,*), . ))
			in *carry // trim record
				do ~( *record, . )
			in ( schema, . )
				do >" *** "
		else
			in ?: %((.,?): *f ): ~EOF
				do >"%s": %?
			in ?: ( *f, . ) // next frame
				do ((*,f), %? )
			else
				in ( schema, . )
					do >" *** "
				do ~( OUTPUT )
	else on ~( OUTPUT )
		in *record:(.,EOF)
			do exit
		else
			do ~( record )
			do ~( schema, . )
			do ~( (*,input), . )
	else on ~( record )
		do ((*,record), (record,*))
		do INPUT


: ( schema, .start )
	.position .event
	on this
		do ((*,position), start )
	else in .READY
		on ((*,record), . )
			do ~( .READY )
			do ((*,event), %((.,?): *record ))
	else on ((*,event), . )
		in *position: '\0'
			do .( COMPLETE, '[' ) // event not consumed
		else in ?: %((?,.): *position )
			in %?: ( '\\', . )
				in %?: ( ., w )
					in *event: /[0-9A-Za-z_]/
						do .CHECK
						do .READY
					else in .CHECK
						do ~( .CHECK )
						do ((*,position), %((.,?):*position))
						do ((*,event), *event )
					else do ~( this )
				else in *event: %((.,?): %? )
					do ((*,position), %((.,?):*position))
				else do ~( this )
			else in %?:' '
				in *event: /[ \t]/
					do .READY
				else
					do ((*,position), %((.,?):*position))
					do ((*,event), *event )
			else in *event: %?
				do ((*,position), %((.,?):*position))
			else do ~( this )
		else // *position is a base entity (singleton) other than '\0'
			do >"Error: %_-terminated schema not supported\n": *position
			do ~( this )
	else on ((*,position), . )
		in ((*,position), '\0' )
			do .( COMPLETE, ']' ) // event is consumed
		else do .READY

