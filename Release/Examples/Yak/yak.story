:
	on init
#		do (( rule, identifier ), ( schema, (a,(b,...((%,id),...(z,'\0')))) ))
#		...
/*
		do (( rule, null ), ( schema, '\0' ))
		do (( *, base ), null )
*/
		do (( rule, hello ), ( schema, (h,(e,(l,(l,(o,'\0'))))) ))
		do (( *, base ), hello )
		do (( *, frame ), ( frame, * ))
		do ( *, input )	// required to catch EOF first frame
		do INPUT

	else in INPUT
		%((rule,.),.)
		on ( INPUT ) // start base rule instance
			in (( rule, *base ), ( schema, . ))
				do ( ((rule,*base),(CONSUMED,*frame)), base )
			else
				in ( rule, *base )
					do >"Error: Yak: rule '%_': no schema found\n":*base
				else do >"Error: Yak: rule '%_' not found\n":*base
				do exit
		else in ?: %( ?:((rule,.),.), base )
			on ( %?, READY )
				do input:"%c"<
			else on (( *, input ), . )
				// could do some preprocessing here
				do (( *, frame ), ( *frame, *input ))
			else on ( %?, COMPLETE )
				do ~( INPUT )
			else on ~( *, input )
				do (( *, frame ), ( *frame, EOF ))
		else on ~( ((rule,.),.), base ) // FAIL
			do ~( INPUT )
	else on ~( INPUT )
		do OUTPUT

	else in ( OUTPUT )
		on ( OUTPUT )
			do ((*,s), base )
			do ((*,f), (frame,*)) // initial frame

		else on ((*,s), %(?,*r)) // s either set together with r or set to successor
			// test if other feeders starting at the same (flag,frame)
			in ?: %( ?:((schema,.),%(*s:(.,?))), *r ): ~*s
				do > " *** Warning: rule '%_': multiple interpretations ***\n": %(*r:((.,?),.))
			in (((schema,.),.), (*s,.))
				// s has predecessor: output s starting event
				in *s:(.,(CONSUMED,.))
					do >: %((.,?):*f)
			else	/* output r begin, knowing that base rule starts
				   with no event to speak of, and other rules always
				   start UNCONSUMED - so no starting event here.
				*/
				in *s: ((schema,'\0'),.) // silence null-schema output
				else
					do >"%%%_:{": %(*r:((.,?),.))

		else in ?: %( ?:((rule,.),(.,*f)), *s ) // s has rule starting this frame
			do ((*,r), %? )
			// set s to the feeder that started at the same (flag,frame)
			in ?: %( ?:((schema,.),%(%?:(.,?))), %? )
				do ((*,s), %? )
#			else error

		else in ?: %( *s, (?:((schema,.),(.,*f)),.)) // *s has successor starting this frame
			do ((*,s), %? ) // set s to successor

		else in ( *s, (.,*f)) // this frame is s's last frame
			// output finishing event, which here cannot be initial frame
			in *s: ((schema,'\0'),.) // silence null-schema output
			else
				in ( *s, (CONSUMED,.))
					do >"%s}": %((.,?):*f)
				else do >"}"

			/* set s to the successor of the schema which the current r
			   fed and which started at finishing (flag,frame) = %(*s,?)
			*/
			in ?: %( %(*r,?:((schema,.),.)), ?:%(?:((schema,.),%(*s,?)),.) )
				do ((*,s), %? )
				do ((*,r), %( %?, ?:((rule,.),.) ))

			// there being no such successor, must imply %( *r, base )
			else in ( *r, base )
				in ( *s, (CONSUMED,.))
					in ?: ( *f, . )
						do ((*,f), %? )
					else
						do ~( OUTPUT )
				do ((*,s), base )
#			else error
		else
			// output event, unless *f is a first CONSUMED schema frame
			in *s:(.,(CONSUMED,*f))
			else in *f:~(frame,*):~(.,EOF)
				do >: %((.,?):*f)

			// move on to next frame
			in ?: ( *f, . )
				do ((*,f), %? )
			else
				do ~( OUTPUT )
	else on ~( OUTPUT )
		// destroys the whole frame structure, including rule
		// and schema instances - all in ONE Consensus cycle
		in *frame:~(.,EOF)
			do >:
			do ~( frame )
		else do exit

	else on ~( frame )
		// we do not want base rule to catch this frame
		do (( *, frame ), ( frame, * ))
		do INPUT


: (( rule, .id ), ( .flag:~schema, .start_frame ))
	%((schema,.),.)

	on ( this )
		// instantiate / subscribe to children schemas - feeders
		do (( %((rule,id),?:(schema,.)), (flag,start_frame)), this )

	else on ~(((schema,.),.), this ) // feeder schema failed
		in %( ?:((schema,.),.), this ): ~%( this, ? )
		else do ~( this ) // FAIL

	else in .COMPLETE
		on ~( this, ((schema,.),.))
			do ~( this ) // FAIL: parent schema failed
	else in .READY
		on (( *, frame ), . )
			do ~( .READY )
			do ~( .(CONSUMED,.) )
			do ~( .(UNCONSUMED,.) )
	else
		on ?: %( %( ?:((schema,.),.), this ), ?:(CONSUMED,.))
			do .( %? ) // TAKE
		on ?: %( %( ?:((schema,.),.), this ), ?:(UNCONSUMED,.))
			do .( %? ) // TAKE

		in %( ?:((schema,.),.), this ): ~%(this,?): ~%(?,COMPLETE)
			in %( ?:((schema,.),.), this ): ~%(this,?): ~%(?,COMPLETE): ~%(?,READY)
			else do .READY	// all children schemas ready
		else do .COMPLETE	// all children schemas complete


: (( schema, .start_position ), ( .flag, .start_frame ))
	.position .event

	on ( this )
		in (((schema,.),.), ( this:((.,'\0'),.), . ))
			// schema has predecessor AND is in null position
			do .( flag, start_frame )
			do .COMPLETE
		else
			do ((*,position), start_position )
			in flag: UNCONSUMED
				do ((*,event), %((.,?):start_frame))
			else do .READY

	else on ~( this, ((rule,.),.))
		do ~( this ) // FAIL: parent rule failed

	else on (( *, event ), . )
		in *position: '\0' // null-schema
			do .( UNCONSUMED, *frame )
			do .COMPLETE
/*
		else in *position: space
			%( .event )
		else in *position: blank
			%( .event )
*/
		else in ?: %((?,.): *position ) // expected @ position
			in %?: ( % , . )
				do ((( rule, %((.,?):%?)), (UNCONSUMED,*frame)), this )
			else in %?: *event
				in %((.,?):*position): '\0'
					do .( CONSUMED, *frame )
					do .COMPLETE
				else
					do ((*,position), %((.,?):*position))
					do .READY
			else do ~( this ) // FAIL
#		else error // *position is a base entity (singleton) other than '\0'
/*
		else %( .event ) // *position is a base entity (singleton) other than '\0'
		// Note that we must have *position:start_position, since we use (: )
		// Issue: we assume there is a narrative for it. How do we make sure?

*/

	else in ?: %( ?:((rule,.),.), this ) // pending on rule
		in .COMPLETE
			on ~( .(((schema,.),.),.) ) // successor schema failed
				in .(((schema,.),.),.)
				else do ~( this ) // FAIL: all successor schemas failed
		else on ( %?, COMPLETE ) // no more TAKE from rule
			in .(((schema,.),.),.) // need successor schema to complete
				do .COMPLETE
			else do ~( this ) // FAIL: no successor schema
		else on ( %?, READY ) // SYNC
			do .READY
		else on (( *, frame ), . )
			do ~( .READY )
		else
			on ?:( %?, (CONSUMED,.)) // TAKE from rule: launch successor schema
				do .(((schema, %((.,?):*position)),%((.,?):%?)), %(this,?:((rule,.),.)))
			on ?:( %?, (UNCONSUMED,.)) // TAKE from rule: launch successor schema
				do .(((schema, %((.,?):*position)),%((.,?):%?)), %(this,?:((rule,.),.)))

	else on ~(((rule,.),.), this )
		do ~( this ) // FAIL: feeder rule failed
	else in .COMPLETE
	else in .READY
		on (( *, frame ), . )
			do ~( .READY )
			do ((*,event), %((.,?):*frame) )
/*
	else on .( PASS, . )
		do .( %(.(PASS,?)), *frame )
		do .COMPLETE
	else on .( FAIL )
		do ~( this )
*/


