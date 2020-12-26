:
	on init
#		do (( Rule, identifier ), ( Schema, (a,(b,...((%,id),...(z,'\0')))) ))
#		do (( Rule, base ), ( Schema, ((%,identifier),'\0') )
/*
		do (( Rule, g ), ( Schema, (h,(e,(l,(l,(o,'\0'))))) ))
		do (( Rule, base ), ( Schema, ((%,g),'\0') ))

		do (( Rule, hello ), ( Schema, (h,(e,(l,(l,(o,'\0'))))) ))
		do (( Rule, hello ), ( Schema, (h,(i,'\0')) ))
		do (( Rule, base ), ( Schema, ((%,hello),'\0') ))

		do (( Rule, identifier ), ( Schema, (('\\',w),'\0') ))
		do (( Rule, base ), ( Schema, ((%,identifier),'\0') ))

		do (( Rule, h ), ( Schema, (h,(e,(l,'\0'))) ))
		do (( Rule, test ), ( Schema, ((%,h),(l,(o,'\0'))) ))
		do (( Rule, base ), ( Schema, ((%,test),'\0') ))

		do (( Rule, hello ), ( Schema, (h,(e,(l,(l,(o,'\0'))))) ))
		do (( Rule, greet ), ( Schema, ((%,hello),'\0') ))
		do (( Rule, base ), ( Schema, ((%,greet),'\0') ))

		do (( Rule, h ), ( Schema, (('\\',w),'\0') ))
		do (( Rule, w ), ( Schema, (w,(o,(r,(l,(d,'\0'))))) ))
		do (( Rule, g ), ( Schema, ((%,h),(',',(' ',((%,w),'\0')))) ))
		do (( Rule, base ), ( Schema, ((%,g),'\0') ))

		do (( Rule, h ), ( Schema, (h,(e,(l,(l,(o,'\0'))))) ))
		do (( Rule, w ), ( Schema, (w,(o,(r,(l,(d,'\0'))))) ))
		do (( Rule, g ), ( Schema, ((%,h),(',',(' ',((%,w),'\0')))) ))
		do (( Rule, base ), ( Schema, ((%,g),'\0') ))

		do (( Rule, h ), ( Schema, (('\\',w),'\0') ))
		do (( Rule, w ), ( Schema, (w,(o,(r,(l,(d,'\0'))))) ))
		do (( Rule, g ), ( Schema, ((%,h),(',',(' ',((%,w),'\0')))) ))
		do (( Rule, base ), ( Schema, ((%,g),'\0') ))

		do (( Rule, ' ' ), ( Schema, (( '\\', ' ' ), '\0' )))
#		do (( Rule, ' ' ), ( Schema, (( %, ' ' ), (( '\\', ' '), '\0' ))))
		do (( Rule, ' ' ), ( Schema, (( '\\', ' ' ), (( %, ' ' ), '\0' ))))
		do (( Rule, g ), ( Schema, (( %, ' ' ), '\0' )))
		do (( Rule, base ), ( Schema, ((%,g),'\0') ))

		do (( Rule, eee ), ( Schema, ( e, '\0' )))
		do (( Rule, eee ), ( Schema, ((%,eee), ( e, '\0' ))))
		do (( Rule, base ), ( Schema, ((%,eee),'\0') ))

		do (( Rule, deadlock ), ( Schema, ((%,deadlock),'\0') ))
		do (( Rule, base ), ( Schema, ((%,deadlock),'\0') ))

		do (( Rule, null ), ( Schema, '\0' ))
		do (( Rule, base ), ( Schema, ((%,null),'\0') ))

*/
		do (( Rule, eee ), ( Schema, ( e, '\0' )))
#		do (( Rule, eee ), ( Schema, ((%,eee), ( e, '\0' )))) // LEFT-recursive
		do (( Rule, eee ), ( Schema, ( e, ((%,eee), '\0' )))) // RIGHT-recursive
		do (( Rule, g ), ( Schema, ((%,eee), ( '.', '\0' ))))
		do (( Rule, base ), ( Schema, ((%,g),'\0') ))

		do ((*,record), (record,*))
		do ( *, input )	// required to catch EOF first frame
		do INPUT

	else in INPUT
		%((rule,.),.)
		on ( INPUT ) // start base rule instance - feeding base
			in ( Rule, base )
				do (((rule,base),(']',(record,*))), base )
			else
				do >"Error: Yak: no base rule\n": *base
				do exit
		else in ?: %( ?:((rule,.),.), base )
			on ( %?, READY )
				do input:"%c"<
			else on (( *, input ), . )
				// could do some preprocessing here
				do ((*,record), (*record,*input))
			else on ~( *, input )
				do ((*,record), (*record,EOF))
			else on ( %?, COMPLETE )
				do ~( INPUT )
		else on ~( ((rule,.),.), base ) // FAIL
			do ~( INPUT )
	else on ~( INPUT )
		in (*,input):~%(?,.)
			do >"(nop)\n"
			do exit
		else do OUTPUT

	else in ( OUTPUT )
		on ( OUTPUT )
			do ((*,s), base )
			do ((*,f), (record,*)) // initial frame

		else on ((*,r), . ) // r pushed or popped
			// test if other feeders starting at s's starting (flag,frame)
			in (((schema,.),%(*s:(((.,?),.),.))), *r ): ~*s
				do >" *** Error: Yak: rule '%_': multiple interpretations ***\n": %(*r:((.,?),.))
				do ~( *, s )
			in ((((schema,.),.),.), *s ) // s has predecessor: r popped
			else in *r:((.,base),.) // base rule pushed: no output
			else in *s: ~(((schema,'\0'),.),.) // r pushed: output r begin
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
					do >" *** Error: Yak: rule '%_': subscriber has no successor ***\n": %(*r:((.,?),.))
					do ~( *, s ) // error
			else // back to base, where s must be successor null-schema
				in (*s,(']',.)) // avoid repetition
					in ?: ( *f, . )
						do ((*,f), %? )
					else do ~(OUTPUT)
				do ~( *, s )
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


: (( rule, .id ), ( .flag, .frame ))
	%(((schema,.),.),.)
	on ( this )
		// instantiate / subscribe to children schemas - feeders
		in ((Rule,id), (Schema,.))
			do (((schema,%((Rule,id),(Schema,?))), (flag,frame)), this )
		else
			do >"Error: Yak: rule '%_': no schema\n": id
			do ~( this )
	else on .EXIT
		do ~( %( ?:((schema,.),.), this ) )
		do ~( this )
	else on ~(((schema,.),.), this ) // feeder schema failed
		in (((schema,.),.), this )
		else do .EXIT // all feeder schemas failed
	else on ~( this, (((schema,.),.),.)) // subscriber schema failed
		in ( this, (((schema,.),.),.))
		else do .EXIT // all subscriber schemas failed
	else in ( this, base )
		in .COMPLETE // chill
		else in .SYNC
			on ~(((schema,.),.),.)
			else on ~((rule,.),.)
			else do .COMPLETE
		else in .READY
			on ((*,record), . )
				do ~( .READY )
		else in (((schema,.),.), this ):~%(?,COMPLETE)
			in (((schema,.),.), this ):~%(?,COMPLETE):~%(?,READY)
			else do .READY // all feeder schemas ready
		else do .SYNC // all feeder schemas complete


: ((( schema, .start ), ( .flag, .frame )), .r:((rule,.),.))
	.position .event
	on ( this )
		in ((((schema,.),.),.), this:(((.,'\0'),.),.))
			// schema has predecessor AND is in null position
			do .( flag, frame ) // TAKE
		else
			do ((*,position), start )
			in flag: '['
				do ((*,event), %((.,?):frame))
	else on .EXIT
		do ~( %((?,.):this) )

	else in ?: %( ?:((rule,.),.), this ) // pending on rule
		in %?: ~%(((schema,.),.), ? ) // let rule initialize
		else in .COMPLETE
			on ~( .(((schema,.),.),.) ) // successor schema failed
				in .(((schema,.),.),.)
				else do .EXIT // all successor schemas failed
		else in .READY
			on ((*,record), . )
				do ~( .READY ) // expecting TAKE from rule or rule schemas
		else on ~.
			in .CYCLIC
				do .EXIT
		else
			on ?: ((((schema,.),.), %? ), (']',.)) // TAKE: launch successor schema
				do .(((schema, %((.,?):*position)), %((.,?):%?)), r )
			on ?: ((((schema,.),.), %? ), ('[',.)) // TAKE: launch successor schema
				do .(((schema, %((.,?):*position)), %((.,?):%?)), r )
			in (((schema,.),.), %? ): ~%(?,CYCLIC): ~%(?,COMPLETE)
				in (((schema,.),.), %? ): ~%(?,CYCLIC): ~%(?,COMPLETE): ~%(?,READY)
				else do .READY // all non-cyclic rule schemas ready
			else in this: %( ?, (((schema,.),.),.)): ~%(?,CYCLIC)
				do .COMPLETE // all non-cyclic rule schemas complete
			else do .EXIT // either cyclic (done) or has no successor
	else on ~( ((rule,.),.), this )
		do .EXIT // feeder rule failed

	else in .COMPLETE // chill
		/* so long as this or r's subscriber schema has successor starting
		   at this finishing (flag,frame)
		*/
		in (((schema,.), %(this,?)), r ): ~this
		else on ~( %(r,?), (((schema,.),%(this,?)),.) )
			in ( %(r,?), (((schema,.),%(this,?)),.) )
			else do .EXIT // defunct
	else in .READY
		on ((*,record), . )
			do ~( .READY )
			do ((*,event), %((.,?):*record) )
	else on ((*,event), . )
		in *position: '\0'
			do .( '[', *record ) // TAKE
		else in ?: %((?,.): *position ) // expected @ current, not terminating position
			in %?: ( %, . )
				in ( Rule, %((.,?):%?) )
					in ((rule,%((.,?):%?)), ('[',*record)) // rule already instantiated
						do .CYCLIC
					do (((rule,%((.,?):%?)), ('[',*record)), this )
				else
					do >"Error: Yak: rule '%_' not found\n": %((.,?):%?)
					do .EXIT
			else in %?: ' '
				in *event: /[ \t]/
					do .READY
				else
					do ((*,position), %((.,?):*position))
					do ((*,event), *event ) // REENTER
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
				else in %?: ( ., s )
					in *event: /[ \t]/
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
			else in *event: %?
				do ((*,position), %((.,?):*position))
			else do .EXIT
		else // *position is a base entity (singleton) other than '\0'
			do >"Error: Yak: %_-terminated schema not supported\n": *position
			do .EXIT
	else on ((*,position), . )
		in *position: '\0'
			do .( ']', *record ) // TAKE
		else in ?: %((?,.):*position):( %, . )
			in ( Rule, %((.,?):%?) )
				in ((rule,%((.,?):%?)), (']',*record)) // rule already instantiated
					do .CYCLIC
				do (((rule,%((.,?):%?)), (']',*record)), this )
			else
				do >"Error: Yak: rule '%_' not found\n": %((.,?):%?)
				do .EXIT
		else do .READY
	else on .( /[\[\]]/, . )
		do .COMPLETE

