/*
	Usage: ./B% -f Scheme/yak.example yak.story < input
	---------------------------------------------------
*/
	on init
		// base rule definition must exist and have non-null schema
		in (( Rule, base ), ( Schema, ~'\0' ))
			do ((*,record), (record,*))
			do ( *, input )	// required to catch EOF first frame
			do INPUT
		else
			do >"Error: Yak: base rule not found or invalid\n"
			do exit

	else in INPUT
		%(((schema,.),.),.)
		on ( INPUT ) // start base rule instance - feeding base
			do (((rule,base), (']',(record,*))), base )
			// instantiate & subscribe to feeder schemas
			do (((schema, %((Rule,base),(Schema,?:~'\0'))), (']',(record,*))), \
				((rule,base), (']',(record,*))))
		else in ?: %( ?:((rule,.),.), base )
			in ( %?, READY )
				on ( %?, READY )
					in ((*,carry), . )
						do ((*,input), *carry )
						do ~( *, carry )
					else do input:"%c"<
				else on ((*,input), . )
					// could do some preprocessing here
					do ((*,record), (*record,*input))
				else on ~( *, input )
					do ((*,record), (*record,EOF))
				else on ((*,record), . )
					do ~( %?, READY )
			else in ( %?, COMPLETE )
				on ~(((schema,.),.),.)
				else on ~((rule,.),.)
				else do ~( INPUT )
			else in (((schema,.),.),%?):~%(?,COMPLETE)
				in (((schema,.),.),%?):~%(?,COMPLETE):~%(?,READY)
				else do ( %?, READY ) // all feeder schemas ready
			else do ( %?, COMPLETE ) // all feeder schemas complete
		else on ~( ((rule,.),.), base ) // FAIL
			in *record: ~((record,*),.) // not first input
				do ((*,carry), %((.,?):*record))
				do ((*,record), %((?,.):*record))
			do ~( INPUT )
	else on ~( INPUT )
		in (*,input): ~%(?,.)
			do >"(nop)\n"
			do exit
		else
			in ((*,carry), . ) // trim record
				do ~( *record, . )
			do OUTPUT

	else in ( OUTPUT )
		on ( OUTPUT )
			do ((*,s), base )
			do ((*,f), (record,*)) // initial frame

		else on ((*,r), . ) // r pushed or popped
			// test if r has other feeders starting at s's starting (flag,frame)
			in (((schema,.),%(*s:((.,?),.))), *r ): ~*s
				do >" *** Error: Yak: rule '%_': multiple interpretations ***\n": %(*r:((.,?),.))
				do ~( *, s )
			in ((((schema,.),.),.), *s ) // s has predecessor: r popped
			else in *r:((.,base),.) // base rule pushed: no output
			else // output r begin
				do >"%%%_:{": %(*r:((.,?),.))

		else in ?: %( ?:((rule,.),('[',*f)), *s ): ~*r // s has rule starting this frame - pushing
			// set s to the feeder starting at r's starting ('[',frame)
			do ((*,r), %? )
			do ((*,s), %(((schema,.),%((.,?):%?)), %? ))

		else in ?: %( ?:((rule,.),(']',*f)), *s ): ~*r // s has rule starting this frame - pushing
			// set s to the feeder starting at r's starting (']',frame)
			in *f: ~(record,*) // output last schema frame
				do >"%s": %((.,?):*f)
			do ((*,r), %? )
			do ((*,s), %(((schema,.),%((.,?):%?)), %? ))

		else in ( *s, (.,*f)) // this frame is s's last frame
			// cyclic case: test if other feeder starting at s's finishing (flag,frame)
			in ?: (((schema,.), %(*s,?)), *r ): ~*s
				in ( *s:~(((schema,'\0'),.),.), (']',.))
					do >"%s": %((.,?):*f)
				do ((*,s), %? )
			else in *r: ~%( ?, base ) // popping
				// output finishing event, which here cannot be initial frame
				in ( *s:~(((schema,'\0'),.),.), (']',.))
					do >"%s}":%((.,?):*f)
				else do >"}"
				/* set s to the successor of the schema which the current r
				   fed and which started at finishing (flag,frame) = %(*s,?)
				*/
				in ?:%( %( *r, ?:(((schema,.),.),.)), ?:(((schema,.),%(*s,?)),.) )
					do ((*,s), %? )
					do ((*,r), %((.,?):%?))
				else // if no such successor, then we must have (*r,base)
					do >" *** Error: Yak: rule '%_': \
						subscriber has no successor ***\n": %(*r:((.,?),.))
					do ~( *, s )
			else // back to base, where s must be successor null-schema
				in (*s,(']',.)) // this frame was consumed
					// right-recursive case: completes on failing next frame
					in ?: %( *f, ? ): ~EOF
						do ((*,carry), %? )
				else // completed unconsumed - cannot be first input
					in ?: %((.,?):*f): ~EOF
						do ((*,carry), %? )
				do ~( OUTPUT )
		else
			// output event, unless *f is a first ']' schema frame
			in *s: ((.,(']',*f)),.)
			else in *f:~(record,*):~(.,EOF)
				do >"%s":%((.,?):*f)
			// move on to next frame
			in ?: ( *f, . )
				do ((*,f), %? )
			else do ~( OUTPUT )
	else on ~( OUTPUT )
		// destroys the whole frame structure, including rule
		// and schema instances - all in ONE Consensus cycle
		in *record:~(.,EOF)
			do ~( record )
		else do exit

	else on ~( record )
		// we do not want base rule to catch this frame
		do ((*,record), (record,*))
		do ~( (*,input), . )
		do INPUT


: ((( schema, .start ), ( .flag, .frame )), .r:((rule,.),.))
	.position .event

	on ( this )
		in this: %((((schema,.),.),.), ?:(((.,'\0'),.),.))
			// schema has predecessor AND is in null position
			do .( flag, frame ) // TAKE
		else
			do ((*,position), start )
			in flag: '['
				do ((*,event), %((.,?):frame))
	else on .EXIT
		in (((schema,.),.), r ): ~%(?,EXIT)
			do ~( this )
		else in ?: (((schema,.),.), r )
			in %?: this // not a MUST, but avoids race condition
				do ~( r ) // all feeder schemas failed

	else in r: ~((.,base),.): ~%( ?, (((schema,.),.),.))
		in ?: (((schema,.),.), r )
			in %?: this // not a MUST, but avoids race condition
				do ~( r ) // all subscribers failed

	else in ?: %( ?:((rule,.),.), this ) // pending on rule
		on ( %?, this )
			in this: ~%(?,CYCLIC) // instantiate rule schemas
				do (((schema, %((Rule,%(((.,?),.):%?)),(Schema,?:~'\0'))), %((.,?):%?)), %? )
		else in .COMPLETE
			on ~( .(((schema,.),.),.) ) // successor schema failed
				in .(((schema,.),.),.)
				else do .EXIT // all successor schemas failed
		else in .READY
			on ((*,record), . )
				do ~( .READY ) // expecting TAKE from rule schemas
		else on ~.
			in .CYCLIC
				do .EXIT
		else
			on ?: ((.,%?), (']',.)) // TAKE: launch successor schema
				do .(((schema, %((.,?):*position)), %((.,?):%?)), r )
			on ?: ((.,%?), ('[',.)) // TAKE: launch successor schema
				do .(((schema, %((.,?):*position)), %((.,?):%?)), r )
			in (((schema,.),.), %? ): ~%(?,CYCLIC): ~%(?,COMPLETE)
				in (((schema,.),.), %? ): ~%(?,CYCLIC): ~%(?,COMPLETE): ~%(?,READY)
				else do .READY // all non-cyclic rule schemas ready
			else in this: %( ?, (((schema,.),.),.)): ~%(?,CYCLIC)
				do .COMPLETE // all non-cyclic rule schemas complete
			else do .EXIT // either cyclic (done) or has no successor
	else on ~( ((rule,.),.), this )
		do .EXIT // feeder rule failed

	else in .COMPLETE /* chill - so long as
		   r's subscriber schema, or, to allow left-recursion, this schema,
		   has a successor starting at this schema's finishing (flag,frame)
		   AND (right-recursion case) no later completion event occurs with
		   a sibling schema starting at the same (flag,frame)
		*/
		on ~( %(r,?), ((.,%(this,?)),.) )
			in ( %(r,?), ((.,%(this,?)),.) )
			else in ((.,%(this,?)), r ): ~this
			else do .EXIT // defunct
		on (((.,'\0'),.), r ) // right-recursion case
			in ((.,%(this:((.,?),.))), r ): ~this
				do .EXIT // defunct
	else in .READY
		on ((*,record), . )
			do ~( .READY )
			do ((*,event), %((.,?):*record) )
	else on ((*,event), . )
		in *position: '\0'
			do .( '[', *record ) // TAKE unconsumed
		else in ?: %((?,.): *position ) // expected @ current, not terminating position
			in %?: ( %, . )
				in %?: ( %, %((Rule,?),(Schema,~'\0')))
					in %?: ( %, %((Rule,?),(Schema,'\0'))) // FORK on null-schema
						do .(((schema, %((.,?):*position)), ('[',*record)), r )
					in ?:((rule,%((.,?):%?)), ('[',*record)) // rule already instantiated
						do ( %?, this )
						do .CYCLIC
					else do (((rule,%((.,?):%?)), ('[',*record)), this )
				else in %?: ( %, %((Rule,?),(Schema,'\0')))
					do ((*,position), %((.,?):*position))
					do ((*,event), *event ) // REENTER
				else
					do >"Error: Yak: rule '%_' not found or invalid\n": %((.,?):%?)
					do .EXIT
			else in %?: ( '\\', . )
				in %?: ( ., w )
					in *event: /[A-Za-z0-9_]/
						do .CHECK
						do .READY
					else in .CHECK
						do ~( .CHECK )
						do ((*,position), %((.,?):*position))
						do ((*,event), *event ) // REENTER
					else do .EXIT
				else in %?: ( ., 0 )
					in *event: '\0'
						do ((*,position), %((.,?):*position))
					else do .EXIT
				else in *event: %((.,?):%?)
					do ((*,position), %((.,?):*position))
				else do .EXIT
			else in %?: ' '
				in *event: /[ \t]/
					do .READY
				else
					do ((*,position), %((.,?):*position))
					do ((*,event), *event ) // REENTER
			else in *event: %?
				do ((*,position), %((.,?):*position))
			else do .EXIT
		else // *position is a base entity (singleton) other than '\0'
			do >"Error: Yak: %_-terminated schema not supported\n": *position
			do .EXIT
	else on ((*,position), . )
		in *position: '\0'
			do .( ']', *record ) // TAKE consumed
		else in ?: %((?,.):*position):( %, . )
			in %?: ( %, %((Rule,?),(Schema,~'\0')))
				in %?: ( %, %((Rule,?),(Schema,'\0'))) // FORK on null-schema
					do .(((schema, %((.,?):*position)), (']',*record)), r )
				in ?:((rule,%((.,?):%?)), (']',*record)) // rule already instantiated
					do ( %?, this )
					do .CYCLIC
				else do (((rule,%((.,?):%?)), (']',*record)), this )
			else in %?: ( %, %((Rule,?),(Schema,'\0')))
				do ((*,position), %((.,?):*position))
			else
				do >"Error: Yak: rule '%_' not found or invalid\n": %((.,?):%?)
				do .EXIT
		else do .READY
	else on .( /[\[\]]/, . )
		do .COMPLETE

