:
	on init
#		do (( rule, identifier ), ( schema, (a,(b,...((%,id),...(z,'\0')))) ))
#		...
/*
		do (( rule, null ), ( schema, '\0' ))
		do (( *, base ), null )

		do (( rule, hello ), ( schema, (h,(e,(l,(l,(o,'\0'))))) ))
		do (( rule, hello ), ( schema, (h,(i,'\0')) ))
		do (( *, base ), hello )

		do (( rule, greet ), ( schema, ((%,hello),'\0') ))
		do (( rule, hello ), ( schema, (h,(e,(l,(l,(o,'\0'))))) ))
		do (( *, base ), greet )
*/
		do (( rule, g ), ( schema, ((%,h),(',',(' ',((%,w),'\0')))) ))
		do (( rule, h ), ( schema, (h,(e,(l,(l,(o,'\0'))))) ))
		do (( rule, w ), ( schema, (w,(o,(r,(l,(d,'\0'))))) ))
		do (( *, base ), g )

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

		else on ((*,s), %(?,*r)) // s set together with r
			// test if other feeders starting at the same (flag,frame)
			in ?: %( ?:((schema,.),%(*s:(.,?))), *r ): ~*s
				do > " *** Warning: rule '%_': multiple interpretations ***\n": %(*r:((.,?),.))
			in (((schema,.),.), (*s,.))
				// s has predecessor: output s starting event
				in *s:(.,(UNCONSUMED,.)): ~((schema,'\0'),.)
					do >"%s": %((.,?):*f)
			else in *s: ~((schema,'\0'),.)
				do >"%%%_:{": %(*r:((.,?),.)) // output r begin

		else on ((*,s), . ) // s set alone
			in ?: %( *s, ?:((rule,.),.)) // set r to rule which s fed
				do >"}" // output r end
				do ((*,r), %? )

		else in ?: %( ?:((rule,.),(.,*f)), *s ) // s has rule starting this frame
			do ((*,r), %? )
			// set s to the feeder that started at the same (flag,frame)
			in ?: %( ?:((schema,.),%(%?:(.,?))), %? )
				do ((*,s), %? )
#			else error

		else in ?: %( *s, (?:((schema,.),(.,*f)),.)) // s has successor starting this frame
			do ((*,s), %? ) // set s to successor

		else in ( *s, (.,*f)) // this frame is s's last frame
			// output finishing event, which here cannot be initial frame
			in ( *s:~((schema,'\0'),.), (CONSUMED,.))
				do >"%s": %((.,?):*f)

			/* set s to the successor of the schema which the current r
			   fed and which started at finishing (flag,frame) = %(*s,?)
			*/
			in ?: %( %(*r,?:((schema,.),.)), ( ?:((schema,.),%(*s,?)), . ))
				do ((*,s), %? )

			// no such successor, therefore we must have %( *r, base )
			else in ?: ( *f, . ) // start flushing
				in ( *s, (CONSUMED,.))
					do ((*,f), (*f,.))
				do ((*,s), base )
			else
				do ~( *, s )

		else on ~( *, s )
			do >"}" // output base rule end
			do ~( OUTPUT )

		else
			// output event, unless *f is a first CONSUMED schema frame
			in *s:(.,(CONSUMED,*f))
			else in *f:~(frame,*):~(.,EOF)
				do >"%s": %((.,?):*f)

			// move on to next frame
			in ?: ( *f, . )
				do ((*,f), %? )
			else
				do ~( OUTPUT )
	else on ~( OUTPUT )
		// destroys the whole frame structure, including rule
		// and schema instances - all in ONE Consensus cycle
		in *frame:~(.,EOF)
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

	else on ~( this, ((schema,.),.))
		do ~( this ) // FAIL: parent schema failed

	else on ~(((schema,.),.), this ) // feeder schema failed
		in %( ?:((schema,.),.), this ): ~%( this, ? )
		else do ~( this ) // FAIL

	else in .COMPLETE // chill
	else in .READY
		on (( *, frame ), . )
			do ~( .READY )
			do ~( .(CONSUMED,.) )
			do ~( .(UNCONSUMED,.) )
	else
		on ?: ( %( ?:((schema,.),.), this ), (CONSUMED,.))
			do .( %((.,?):%?) ) // TAKE
		on ?: ( %( ?:((schema,.),.), this ), (UNCONSUMED,.))
			do .( %((.,?):%?) ) // TAKE

		in %( ?:((schema,.),.), this ): ~%(this,?): ~%(?,COMPLETE)
			in %( ?:((schema,.),.), this ): ~%(this,?): ~%(?,COMPLETE): ~%(?,READY)
			else do .READY	// all children schemas ready
		else do .COMPLETE	// all children schemas complete


: (( schema, .start_position ), ( .flag, .start_frame ))
	.position .event

	on ( this )
		in (((schema,.),.), ( this:((.,'\0'),.), . ))
			// schema has predecessor AND is in null position
			do .( flag, start_frame ) // TAKE
			do .COMPLETE
		else
			do ((*,position), start_position )
			in flag: UNCONSUMED
				do ((*,event), %((.,?):start_frame))
			else do .READY

	else on ~( this, ((rule,.),.))
		do ~( this ) // FAIL: parent rule failed

	else on ~(((rule,.),.), this )
		do ~( this ) // FAIL: feeder rule failed (if there was one)

	else in ?: %( ?:((rule,.),.), this ) // pending on rule
		in .COMPLETE
			on ~( .(((schema,.),.),.) ) // successor schema failed
				in .(((schema,.),.),.)
				else do ~( this )   // all successor schemas failed
		else on ( %?, READY ) // SYNC
			do .READY // propagate to parent rule
		else on (( *, frame ), . )
			do ~( .READY ) // expecting TAKE from rule
		else
			on ( %?, COMPLETE ) // last possible TAKE from rule
				do .COMPLETE
			on ?:( %?, (CONSUMED,.)) // TAKE from rule: launch successor schema
				do .(((schema, %((.,?):*position)),%((.,?):%?)), %(this,?:((rule,.),.)))
			on ?:( %?, (UNCONSUMED,.)) // TAKE from rule: launch successor schema
				do .(((schema, %((.,?):*position)),%((.,?):%?)), %(this,?:((rule,.),.)))

	else in .COMPLETE // chill
	else in .READY
		on (( *, frame ), . )
			do ((*,event), %((.,?):*frame) )
			do ~( .READY )
	else on (( *, event ), . )
		in *position: '\0' // null-schema or base rule's schema finishing
			do .( UNCONSUMED, *frame ) // TAKE
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
					do .( CONSUMED, *frame ) // TAKE
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
	else on .( PASS, . )
		do .( %(.(PASS,?)), *frame ) // TAKE
		do .COMPLETE
	else on .( FAIL )
		do ~( this )
*/

