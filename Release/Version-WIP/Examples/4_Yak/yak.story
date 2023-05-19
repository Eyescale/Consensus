/*
	Usage: ../../B% -f Schemes/yak.ini yak.story < input
	----------------------------------------------------
*/
	on init
		// base rule definition must exist and have non-null schema
		in (( Rule, base ), ( Schema, ~'\0' ))
			do : record : (record,*)
			do : state : IN
		else
			do >"Error: Yak: base rule not found or invalid\n"
			do exit

	else in : state : IN
		on : state : . // start base rule instance - feeding base
			do (((rule,base), (']',*record)) | {
				(((schema, %((Rule,base),(Schema,?:~'\0'))), %(%|:(.,?))), %| ),
				.( %|, base ) } )
		else in .( ?, base )
			%( . )
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
				on ~.: . < { ., %% }
					do : state : OUT
			else in (.,%?): ~%(?,DONE)
				in (.,%?): ~%(?,DONE): ~%(?,READY)
				else do ( %?, READY ) // all feeder schemas ready
			else do ( %?, DONE ) // all feeder schemas complete
		else on ~( ., base ) // FAIL
			in : record : (((record,*),.),.) // not first input
				do : record : %(*record:(?,.))
				do : carry : %(*record:(.,?))
			do : state : OUT

	else in : state : OUT
		.s .f .r
		on : state : .
			in : record : (record,*)
				do >"(nop)\n"
				do exit
			else
				do : s : base
				do : f : (record,*) // initial frame
				in : carry : . // trim record
					do ~( *record, . )

		else on : pop : ? // popping
			do : pop : ~.
			// output last schema event, providing it's not initial frame
			in ?: %(*s:(((.,~'\0'),.),.),(']',(.,?))): ~(record,*)
				do >"%s}":%?
			else do >"}"
			in ( *r, base )
				do : state : ~.
				in : f : ( record, * ) // successor null-schema
					in : input : ?
						do >"%s": %?
					else do >"\n" // EOF
				else in : f : ((record,*),.) // first = last frame - no carry
					in : input : ?
						do >"%s": %?
					else do >"\n" // EOF
				else in %?: ('[',.) // completed unconsumed
					in ?: %(*f:(.,?:~EOF))
						do : carry : %?
					else do >"\n"
				else in ( *f, ?:~EOF )
					// right-recursive case: completes on failing next frame
					do : carry : %?
			else
				/* set s to the successor of the schema which the current r
				   fed and which started at s's finishing frame */
				in ( %(*r,?), ?:(((schema,.),%?),.) )
					do : s : %?
					do : r : %(%?:(.,?))
				else // if no such successor, then we must have (*r,base)
					do >" *** Error: Yak: rule '%_': "
						"subscriber has no successor ***\n": %(*r:((.,?),.))
					do : s : ~.

		else on : r : ? // r pushed or popped
			/* --------------------------------------------------------------
			// test if r has other feeders starting at s's starting frame
			in ( (.,%(*s:((.,?),.))), %? ): ~*s
				do >" *** Warning: Yak: rule '%_': "
					"multiple interpretations ***\n": %(%?:((.,?),.))
			----------------------------------------------------------------- */
			in ((.,*r),*s) // s has predecessor: r popped
			else // output r begin
				do >"%%%s:{": %(%?:((.,?),.))

		// s has rule with event consumed starting this frame => pushing or popping
		else in ( (.,?:('[',*f)):~*r, *s )
			// rule has schema not starting & finishing both at the same frame => pushing
			in ?: ((.,%?), %(?:(.,%?):~*r,*s)): ~%(?,%?)
				// set s to the feeder starting & not finishing at r's starting frame
				do : s : %?
				do : r : %(%?:(.,?))
			else do : pop : %?

		// s has rule with event unconsumed starting this frame => pushing or popping
		else in ( (.,?:(']',*f)):~*r, *s )
			// rule has schema not both starting & finishing at the same frame => pushing
			in ?: ((.,%?), %(?:(.,%?):~*r,*s)): ~%(?,%?)
				// output last schema event, providing it's not also starting
				in *f: ~%(*s:((.,(']',?)),.)): ~(record,*)
					do >"%s": %(*f:(.,?))
				// set s to the feeder starting & not finishing at r's starting frame
				do : s : %?
				do : r : %(%?:(.,?))
			else do : pop : %?

		else in ( *s, ?:(.,*f)) // this frame is s's last frame => popping
			// test if other, non-predecessor schema starting at s's finishing frame
			in ?: ( (.,%?), *r ): ~%(?,%?): ~%(?,*s)
				in ( *s:(((.,~'\0'),.),.), (']',.))
					do >"%s": %(*f:(.,?))
				do : s : %?
			else do : pop : %?

		else // moving on
			in ?:( *f, . ) // there is a next frame
				// output event, unless *f is a first ']' schema frame
				in *f: (.,?): ~(record,*): ~%(*s:((.,(']',?)),.))
					do >"%s": %(*f:(.,?))
				do : f : %?
			else
				in *f: (.,?:~EOF): ~(record,*)
					do >"%s": %?
				do : state : ~.

	else in : state : ~.
		on : state : ~.
			// destroy the whole record structure, including rule
			// and schema instances - all in ONE Consensus cycle
			in : record : ~(.,EOF)
				do ~( record )
			else do exit
		else on ~( record )
			// reset: we do not want base rule to catch this frame
			do : record : (record,*)
			do ~( input )
			do : state : IN


.s: ((( schema, .position ), .start ), .r )
	.p
	on ( s )
		// schema is in null position AND has predecessor
		in ( position:'\0' ? ((.,r), s ) :)
			do .( start ) // TAKE as-is
		else in start: ('[',(.,?)) // event to be consumed
			do : p : position
			do : s : !! Take( ((*,p),.position), ((*,event),%?) )
		else
			do : p : position
			do : s : !! Take( ((*,p),.position) )
	else on .EXIT
		in ~.: (.,r): ~%(?,EXIT) // all r feeder schemas failed
			do ~( r )
		else do ~( s )
/*
	else in ~.: (r,.) // all r subscribers failed
		do ~( r )
*/
	else in .DONE
		in .( ., s ) // s was pending on rule
			on ~( .(.,r) )
				in .(.,r) // s still has successor
				else do .EXIT // defunct
		else on ~( %(r,?), . ) // r's subscribers connections changed
			in ( %(r,?), ((.,%(.?)),.) )
			else // none of r's subscribers has a successor starting at s's finish frame
				in ((.,%(.?)), r ) :~s // :~%(?,DONE)
				else // r has no feeder other than s starting at s's finish frame
					do .EXIT // defunct
	else in .( ?, s ) // s is pending on rule
		on ~.: . < { %%, . }
			in .CYCLIC
				do > "Warning: Yak: unlocking rule '%_'\n": %(r:((.,?),.))
				do .EXIT
		else in .READY
			on : record : .
				do ~( .READY )
		else	// expecting TAKE from rule schemas
			on ((.,%?), ?:('[',.)) // TAKE: launch successor schema
				do .(((schema, %(*p:(.,?))), %? ), r )
				on ((.,%?), ?:(']',.)) // TAKE: launch successor schema
					do .(((schema, %(*p:(.,?))), %? ), r )
			else on ((.,%?), ?:(']',.)) // TAKE: launch successor schema
				do .(((schema, %(*p:(.,?))), %? ), r )
			else // sync based on rule schemas
				in (.,%?): ~%(?,CYCLIC): ~%(?,DONE)
					in (.,%?): ~%(?,CYCLIC): ~%(?,DONE): ~%(?,READY)
					else do .READY // all non-cyclic rule schemas ready
				else // all non-cyclic rule schemas complete
					do ( .CYCLIC ? .EXIT : .(.,r) ? .DONE : .EXIT )
	else on ~( .(.,s) ) // feeder rule failed
		do .EXIT
	else on .( /[[\]]/, . ) // final TAKE done
		do .DONE
	else in .READY // s pending on user input
		on : record : (.,?)
			do ~( .READY )
			do ( *s, %? )
	else
		on READY < *s // TAKE and move on
			do .READY
			do : p : %(*p:(.,?))
		else on ~( TAKE ) < *s // next input
			do .READY
		else on :( TAKE, ? ): ~. < *s // TAKE final, position no longer relevant
			do .( %<?>, *record )
		else on :( TAKE, ? ): . < *s // TAKE rule
			in : p : ( ~(%,.), ? ) // event consumed, other than first
				do : p : %?
			in ((Rule,?:%<!:(.,?)>),(Schema,~'\0'))
				in ?: ((rule,%?), (%<?>,*record)) // rule already instantiated
					do .( %?, s )
					do .CYCLIC
				else do (((rule,%?), (%<?>,*record)) | {
					(((schema, %((Rule,%?),(Schema,?))), %(%|:(.,?))), %| ),
					.( %|, s ) } )
			else
				do >"Error: Yak: rule '%_' not found or invalid\n": %<!:(.,?)>
				do .EXIT
		else on exit < *s // FAIL
			do .EXIT

: Take
	on ~( ., %% ) < ..
		do exit
	else in READY
		on ~( %%, ? ) < ..
			do : event : %<?>
			do ~( READY )
	else on : event : .
		in : p : '\0'
			do :( TAKE, '[' ): ~. // TAKE, unconsumed
		else in : p : (?,.) // expected @ current, not terminating position
			in %?: ( %, ? )
				do :( TAKE, '[' ): %? // TAKE, unconsumed
			else in %?: ( '\\', ? )
				in %?: 0
					in : event : '\0'
						do : p : %(*p:(.,?))
					else do exit // FAIL
				else in %?: d
					in : event : /[0-9]/
						do : p : %(*p:(.,?))
					else do exit // FAIL
				else in %?: w
					in : event : /[A-Za-z0-9_]/
						do : p : %(*p:(.,?))
					else do exit // FAIL
				else in %?: i
					in : event : /[0-9]/
						do TAKE~
					else
						do : p : %(*p:(.,?))
						do : event : *event // REENTER
				else in : event : %?
					do : p : %(*p:(.,?))
				else do exit // FAIL
			else in %?: ' '
				in : event : /[ \t]/
					do TAKE~
				else
					do : p : %(*p:(.,?))
					do : event : *event // REENTER
			else in : event : %?
				do : p : %(*p:(.,?))
			else do exit // FAIL
		else // *p is a base entity (singleton) other than '\0'
			do >"Error: Yak: %_-terminated schema not supported\n": *p
			do exit // FAIL
	else on : p : ?
		in %?: '\0'
			do :( TAKE, ']' ): ~. // TAKE, consumed
		else in %?: (( %, ? ), . )
			do :( TAKE, ']' ): %? // TAKE, consumed or first
		else on init // no moving on
			do TAKE~
		else do READY
	else on ~( TAKE )
		do READY
	else on ( TAKE, . )
		do exit

