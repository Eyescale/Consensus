/*
	Usage: ./B% -f Scheme/yak.ini yak.story < input
	-----------------------------------------------
*/
	on init
		// base rule definition must exist and have non-null schema
		in (( Rule, base ), ( Schema, ~'\0' ))
			do : record : (record,*)
			do INPUT
		else
			do >"Error: Yak: base rule not found or invalid\n"
			do exit

	else in INPUT
		%( . )
		on ( INPUT ) // start base rule instance - feeding base
			do (((rule,base), (']',*record)) | {
				(((schema, %((Rule,base),(Schema,?:~'\0'))), %(%|:(.,?))), %| ),
				.( %|, base ) } )
		else in .( ?, base )
			in ( %?, READY )
				on ( %?, READY )
					in : carry : ?
						do : input : %?
						do : carry : ~.
					else do input:"%c"<
				else on : input : ?
					// could do some preprocessing here
					do : record : (*record,%?)
				else on : input : ~.
					do : record : (*record,EOF)
				else on : record : .
					do ~( %?, READY )
			else in ( %?, DONE )
				on ~(((schema,.),.),.)
				else on ~((rule,.),.)
				else do ~( INPUT )
			else in (((schema,.),.),%?): ~%(?,DONE)
				in (((schema,.),.),%?): ~%(?,DONE): ~%(?,READY)
				else do ( %?, READY ) // all feeder schemas ready
			else do ( %?, DONE ) // all feeder schemas complete
		else on ~( ., base ) // FAIL
			in : record : ~((record,*),.) // not first input
				do : record : %(*record:(?,.))
				do : carry : %(*record:(.,?))
			do ~( INPUT )
	else on ~( INPUT )
		in ~.: input
			do >"(nop)\n"
			do exit
		else
			in : carry : . // trim record
				do ~( *record, . )
			do OUTPUT

	else in ( OUTPUT )
		.s .f .r
		on ( OUTPUT )
			do : s : base
			do : f : (record,*) // initial frame

		else on : r : ? // r pushed or popped
			// test if r has other feeders starting at s's starting (flag,frame)
			in (((schema,.),%(*s:((.,?),.))), %? ): ~*s
				do >" *** Error: Yak: rule '%_': multiple interpretations ***\n": %(%?:((.,?),.))
				do : s : ~.
			in ((((schema,.),.),.), *s ) // s has predecessor: r popped
			else in %?:((.,base),.) // base rule pushed: no output
			else // output r begin
				do >"%%%_:{": %(%?:((.,?),.))

		else in ( ?:((rule,.),('[',*f)):~*r, *s ) // s has rule starting this frame - pushing
			// set s to the feeder starting at r's starting ('[',frame)
			do : r : %?
			do : s : %(((schema,.),%(%?:(.,?))), %? )

		else in ( ?:((rule,.),(']',*f)):~*r, *s ) // s has rule starting this frame - pushing
			// set s to the feeder starting at r's starting (']',frame)
			in *f: ~(record,*) // output last schema frame
				do >"%s": %(*f:(.,?))
			do : r : %?
			do : s : %(((schema,.),%(%?:(.,?))), %? )

		else in ( *s, (.,*f)) // this frame is s's last frame
			// cyclic case: test if other feeder starting at s's finishing (flag,frame)
			in ?: (((schema,.), %(*s,?)), *r ): ~*s
				in ( *s:~(((schema,'\0'),.),.), (']',.))
					do >"%s": %(*f:(.,?))
				do : s : %?
			else in *r: ~%( ?, base ) // popping
				// output finishing event, which here cannot be initial frame
				in ( *s:~(((schema,'\0'),.),.), (']',.))
					do >"%s}":%(*f:(.,?))
				else do >"}"
				/* set s to the successor of the schema which the current r
				   fed and which started at finishing (flag,frame) = %(*s,?)
				*/
				in ( %(*r,?:(((schema,.),.),.)), ?:(((schema,.),%(*s,?)),.) )
					do : s : %?
					do : r : %(%?:(.,?))
				else // if no such successor, then we must have (*r,base)
					do >" *** Error: Yak: rule '%_': \
						subscriber has no successor ***\n": %(*r:((.,?),.))
					do : s : ~.
			else // back to base, where s must be successor null-schema
				in (*s,(']',.)) // this frame was consumed
					// right-recursive case: completes on failing next frame
					in ( *f, ?:~EOF )
						do : carry : %?
				else in *f:( ., ?:~EOF )
					// completed unconsumed - cannot be first input
					do : carry : %?
				do ~( OUTPUT )
		else
			// output event, unless *f is a first ']' schema frame
			in *s: ((.,(']',*f)),.)
			else in *f:( ., ?:~EOF ):~(record,*)
				do >"%s":%?
			// move on to next frame
			in ?: ( *f, . )
				do : f : %?
			else do ~( OUTPUT )
	else on ~( OUTPUT )
		// destroy the whole record structure, including rule
		// and schema instances - all in ONE Consensus cycle
		in : record : ~(.,EOF)
			do ~( record )
		else do exit

	else on ~( record )
		// reset: we do not want base rule to catch this frame
		do : record : (record,*)
		do ~( input )
		do INPUT


.s: ((( schema, .position ), .start ), .r )
	.p
	on ( s )
		// schema has predecessor AND is in null position
		in ( position:'\0' ? ((((schema,.),.),.), s ) :)
			do .( start ) // TAKE as-is
		else
			do : p : position
			do : s : !! Take( ((*,p), .position ),
				( start:('[',.) ? ((*,event), %(start:(.,(.,?)))) :) )
	else on .EXIT
		in (((schema,.),.), r ): ~%(?,EXIT)
			do ~( s )
		else do ~( r ) // all r feeder schemas failed
	else on ~( .(.,s) )
		do .EXIT // feeder rule failed
	else in ~.: ( (r,base) ?: ( r, (((schema,.),.),.)) )
		do ~( r ) // all r subscribers failed
	else in .DONE
		in .( ?, s ) // s has successor schema
			on ~( s, (((schema,.),.),.) ) // successor schema failed
				in ( s, (((schema,.),.),.) )
				else do .EXIT // all successor schemas failed
		else	/* chill - so long as
			r's subscriber schema, or, to allow left-recursion, this schema,
			has a successor starting at this schema's finishing (flag,frame)
			AND (right-recursion case) no later completion event occurs with
			a sibling schema starting at the same (flag,frame) */
			on ~( %(r,?), ((.,%(s,?)),.) )
				in ( %(r,?), ((.,%(s,?)),.) )
				else in ((.,%(s,?)), r ): ~s
				else do .EXIT // defunct
			on (((.,'\0'),.), r ) // right-recursion case
				in ((.,start), r ): ~s
					do .EXIT // defunct
	else on .( /[[\]]/, . )
		do .DONE
	else
		in .READY
			on : record : .
				do ~( .READY )
		else in .( ?, s ) // s pending on rule
			on ~.: . < { %%, . }
				in .CYCLIC
					do > "Warning: Yak: unlocking rule '%_'\n": %(r:((.,?),.))
					do .EXIT
			else	// expecting TAKE from rule schemas
				on ((.,%?), ?:(']',.)) // TAKE: launch successor schema
					do .(((schema, %(*p:(.,?))), %? ), r )
				on ((.,%?), ?:('[',.)) // TAKE: launch successor schema
					do .(((schema, %(*p:(.,?))), %? ), r )
				in (((schema,.),.), %? ): ~%(?,CYCLIC): ~%(?,DONE)
					in (((schema,.),.), %? ): ~%(?,CYCLIC): ~%(?,DONE): ~%(?,READY)
					else do .READY // all non-cyclic rule schemas ready
				else in ( s:~%(?,CYCLIC), (((schema,.),.),.) )
					do .DONE // all non-cyclic rule schemas complete
				else do .EXIT // either cyclic (done) or has no successor
		else
			on : ( TAKE, ? ) : ~. < *s
				do .( %<?>, *record ) // TAKE, consumed or unconsumed
			else on : ( TAKE, ? ) : . < *s
				in ((Rule,%<!>),(Schema,~'\0'))
					in ((Rule,%<!>),(Schema,'\0')) // FORK on null-schema
						do .(((schema, %(*p:(.,?))), (%<?>,*record)), r )
					in ?: ((rule,%<!>), (%<?>,*record)) // rule already instantiated
						do .( %?, s )
						do .CYCLIC
					else do (((rule,%<!>), (%<?>,*record)) | {
						(((schema, %((Rule,%<!>),(Schema,?:~'\0'))), %(%|:(.,?))), %| ),
						.( %|, s ) } )
				else in ((Rule,%<!>),(Schema,'\0')) // moving on
					do : p : %(*p:(.,?))
					do : s : !! Take( ((*,p),%(*p:(.,?))),
						( %<?:'['> ? ((*,event),%(*record:(.,?))) :) )
				else
					do >"Error: Yak: rule '%_' not found or invalid\n": %<!>
					do .EXIT
			else on MORE~ < *s
				do .READY
			else on NEXT < *s
				do : p : %(*p:(.,?))
				do .READY
			else on exit < *s // FAIL
				do .EXIT

: Take
	in NEXT
		on : record : ?  < ..
			do : event : %<?:(.,?)>
			do ~( NEXT )
	else on : event : .
		in : p : '\0'
			do : ( TAKE, '[' ) : ~. // TAKE, event unconsumed
		else in : p : (?,.) // expected @ current, not terminating position
			in %?: ( %, ? )
				do : ( TAKE, '[' ) : %?
			else in %?: ( '\\', ? )
				in %?: w
					in : event : /[A-Za-z0-9_]/
						do check
						do MORE~
					else in check
						do ~( check )
						do : p : %(*p:(.,?))
						do : event : *event // REENTER
					else do exit // FAIL
				else in %?: 0
					in : event : '\0'
						do : p : %(*p:(.,?))
					else do exit // FAIL
				else in : event : %?
					do : p : %(*p:(.,?))
				else do exit // FAIL
			else in %?: ' '
				in : event : /[ \t]/
					do MORE~
				else
					do : p : %(*p:(.,?))
					do : event : *event // REENTER
			else in : event : %?
				do : p : %(*p:(.,?))
			else do exit // FAIL
		else // *p is a base entity (singleton) other than '\0'
			do >"Error: Yak: %_-terminated schema not supported\n": *p
			do exit // FAIL
	else on MORE~
		do NEXT
	else on : p : ?
		in %?: '\0'
			do : ( TAKE, ']' ): ~. // TAKE, event consumed
		else in	%?: (( %, ? ), . )
			do : ( TAKE, ']' ): %?
		else do NEXT
	else on ( TAKE, . )
		do exit

