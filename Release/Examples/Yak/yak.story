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

		do (( rule, word ), ( schema, (('\\',w),'\0') ))
		do (( *, base ), word )

		do (( rule, deadlock ), ( schema, ((%,deadlock),'\0') ))
		do (( *, base ), deadlock )
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
			in ( rule, *base )
				do ( ((rule,*base),(CONSUMED,*frame)), base )
			else
				do >"Error: Yak: rule '%_' not found\n":*base
				do exit
		else in ?: %( ?:((rule,.),.), base )
			on ( %?, READY )
				do input:"%c"<
			else on (( *, input ), . )
				// could do some preprocessing here
				do (( *, frame ), ( *frame, *input ))
			else on ~( *, input )
				do (( *, frame ), ( *frame, EOF ))
			else on ( %?, COMPLETE )
				do ~( INPUT )
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
				do > " *** Warning: Yak: rule '%_': multiple interpretations ***\n": %(*r:((.,?),.))
			in (((schema,.),.), (*s,.))
				// s has predecessor: output s starting event
				in *s:(.,(UNCONSUMED,.)): ~((schema,'\0'),.)
					do >"%s": %((.,?):*f)
			else in *s: ~((schema,'\0'),.)
				do >"%%%_:{": %(*r:((.,?),.)) // output r begin

		else in ?: %( ?:((rule,.),(.,*f)), *s ) // s has rule starting this frame
			do ((*,r), %? )
			// set s to the feeder which started at the same (flag,frame)
			do ((*,s), %( ?:((schema,.),%(%?:(.,?))), %? ))

		else in ?: %( *s, (?:((schema,.),(.,*f)),.)) // s has successor starting this frame
			do ((*,s), %? ) // set s to successor

		else in ( *s, (.,*f)) // this frame is s's last frame
			// output finishing event, which here cannot be initial frame
			in ( *s:~((schema,'\0'),.), (CONSUMED,.))
				do >"%s}": %((.,?):*f)
			else in ( %(?:*s,(UNCONSUMED,.)), %(?:*r,base) )
				// special case: null-schema with no predecessor
				in *s:((schema,'\0'),.):~%(((schema,.),.),(?,.))
					in *f:~(.,EOF)
						do >"%s": %((.,?):*f)
				else in *f:~(.,EOF)
					do >"}%s": %((.,?):*f)
				else do >"}"
			else do >"}"

			/* set s to the successor of the schema which the current r
			   fed and which started at finishing (flag,frame) = %(*s,?)
			*/
			in ?: %( %(*r,?:((schema,.),.)), ( ?:((schema,.),%(*s,?)), . ))
				do ((*,s), %? )
				do ((*,r), %( %?, ?:((rule,.),.)) )
			else // if no such successor, then we must have (*r,base)
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
		in ((rule,id), (schema,.))
			do (( %((rule,id),?:(schema,.)), (flag,start_frame)), this )
		else
			do >"Error: Yak: rule '%_': no schema found\n": id
			do ~( this )
	else on ~( this, ((schema,.),.))
		do ~( this ) // FAIL: parent schema failed
	else on ~(((schema,.),.), this ) // feeder schema failed
		in %( ?:((schema,.),.), this ): ~%( this, ? )
		else do ~( this ) // all feeder schemas failed
	else in .COMPLETE // chill
	else in .READY
		on (( *, frame ), . )
			do ~( .(UNCONSUMED,.) )
			do ~( .(CONSUMED,.) )
			do ~( .READY )
	else in %( ?:%((schema,.),.), this ): ~%(this,?)
		on ( %( ?:((schema,.),.), this ), (CONSUMED,.))
			do .( CONSUMED, *frame ) // TAKE
		on ( %( ?:((schema,.),.), this ), (UNCONSUMED,.))
			do .( UNCONSUMED, *frame ) // TAKE
		in %( ?:((schema,.),.), this ): ~%(this,?): ~%(?,COMPLETE)
			in %( ?:((schema,.),.), this ): ~%(this,?): ~%(?,COMPLETE): ~%(?,READY)
			else do .READY // all children schemas ready
		else do .COMPLETE // all children schemas complete


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
				else do ~( this ) // all successor schemas failed
		else on ~. // system is deadlocked
			do ~( this )
		else on ( %?, READY ) // SYNC
			do .READY // propagate to parent rule
		else on (( *, frame ), . )
			do ~( .READY ) // expecting TAKE from rule
		else
			on ( %?, (CONSUMED,.)) // TAKE from rule: launch successor schema
				do .(((schema, %((.,?):*position)),(CONSUMED,*frame)), %(this,?:((rule,.),.)))
			on ( %?, (UNCONSUMED,.)) // TAKE from rule: launch successor schema
				do .(((schema, %((.,?):*position)),(UNCONSUMED,*frame)), %(this,?:((rule,.),.)))
			on ( %?, COMPLETE ) // no more TAKE from rule
				do .COMPLETE
	else in .COMPLETE // chill
	else in .READY
		on (( *, frame ), . )
			do ((*,event), %((.,?):*frame) )
			do ~( .(PASS,.) )
			do ~( .FAIL )
			do ~( .READY )
	else on (( *, event ), . )
		in *position: '\0'
			do .( UNCONSUMED, *frame ) // TAKE
			do .COMPLETE
		else in ?: %((?,.): *position ) // expected @ current, not terminating position
			in %?: ( %, . )
				in ?: ( rule, %((.,?):%?) )
					do (( %?, (UNCONSUMED,*frame)), this )
				else
					do >"Error: Yak: rule '%_' not found\n": %((.,?):%?)
					do ~( this )
			else in %?: ( '\\', . )
				in %?: ( ., ' ' )
					in *event: ' '
						do .( PASS, CONSUMED )
					else do .FAIL
				else in %?: ( ., t )
					in *event: '\t'
						do .( PASS, CONSUMED )
					else do .FAIL
				else in %?: ( ., w )
					in *event: /[A-Za-z0-9_]/
						do .CHECK
						do .READY
					else in .CHECK
						do ((*,position), %((.,?):*position))
						do ((*,event), *event ) // REENTER
						do ~( .CHECK )
					else do .FAIL
			else in %?: /[ \t]/
				in *event: /[ \t]/
					do .READY
				else
					do ((*,position), %((.,?):*position))
					do ((*,event), *event ) // REENTER
			else
				in *event: %?
					do .( PASS, CONSUMED )
				else do .FAIL
		else // *position is a base entity (singleton) other than '\0'
			do >"Error: Yak: %_-terminated schema not supported\n": *position
			do ~( this )
	else on .( PASS, . )
		in %((.,?):*position): '\0'
			do .( %(.(PASS,?)), *frame ) // TAKE
			do .COMPLETE
		else
			do ((*,position), %((.,?):*position))
			do .READY
	else on .( FAIL )
		do ~( this )

