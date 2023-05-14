: Yak
	on init
		// base rule definition must exist and have non-null schema
		in (( Rule, base ), ( Schema, ~'\0' ))
			do : record : (record,*)
			do : state : IN
		else
			do >&"Error: Yak: base rule not found or invalid\n"
			do exit

	else in : state : IN  // input
		on : state : . // start base rule instance - feeding base
			do (((rule,base), (']',*record)) | {
				(((schema, %((Rule,base),(Schema,?:~'\0'))), %(%|:(.,?))), %| ),
				.( %|, base ) } )
		else in .( ?, base )
			%( . )
			in ( %?, READY )
				on ( %?, READY )
					do ( .., READY )
				else on : input : ? < ..
					// could do some preprocessing here
					do : record : (*record,%<?>)
				else on : input : ~. < ..
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
			do ( .., OUT )
			do : state : OUT

	else in : state : OUT  // traversal
		.s .f .r
		on : state : .
			in : record : (record,*)
				do >&"(nop)\n"
				do exit
			else
				do : f : (record,*) // initial frame
				do : s : base

		// pending on parent(=driver)'s request
		else in : pending : ?
			on ~( %%, ? ) < ..
+			in %?: OUT // no matter what
				do : pending : ~.
				do : state : ~.
				do : push : ~.
				do : pop : ~.
			else in %<?:( CONTINUE )>
				in %?:( IN, ? ) // after push notification
					// set s to the feeder starting at candidate's starting frame
					do :< r, s >:< %?, %((.,%(%?:(.,?))),%?) >
				else in %?:( OUT, ? ) // after pop notification
					// set r to candidate's subscriber rule
					do :< r, s >:< %(%?:(.,?)), %? >
				else in %?:( TAKE, . ) // after event notification
					in : push : OUT
						do : state : ~.
						do : push : ~.
					else in : push : ?
						do : pending : (IN,%?)
						do : push : ~.
					else in : pop : ?
						do : pending : ((%?:OUT) ?: (OUT,%?))
						do : pop : ~.
			else in %<?:( PRUNE )>
				in %?:( IN, ? ) // after push notification
					// jump to candidate's finish frame and schema
					do : r : %? // candidate is rule
					do : s : %(?:(.,%?),(.,.))
					do : f : %((.,%?),(.,?))
					do : push : OUT
				else in %?:( OUT, ? ) // after pop notification
					// pop to candidate's rule and jump to finish
					// frame and schema
					in %?:(.,?) // candidate is schema thread
						do : r : %?
						do : s : %(?:(.,%?),(.,.))
						do : f : %((.,%?),(.,?))
						do : push : OUT
				else in %?:( TAKE, . ) // after event notification
					in : push : OUT
						do : pending : OUT
						do : push : ~.
					else in : pop : ?
						do : pending : ((%?:OUT) ?: (OUT,%?))
						do : pop : ~.
					else in : r : ?
						// jump to finish frame and schema
						do : s : %(?:(.,%?),(.,.))
						do : f : %((.,%?),(.,?))
						do : push : OUT
			else in %<?:( DONE )>
				do : state : ~.
				do : push : ~.
				do : pop : ~.

			do ~( %? ) // release last pending condition

		else on : pending : . // send parent(=driver) notification
			in %?:( TAKE, ? )
				do (( .., TAKE ), %? )
			else in %?:( IN, ? )
				do (( .., IN ), %(((.,?),.):%?) )
			else in %?:( OUT, ? )
				do (( .., OUT ), %(((.,?),.):%(%?:(.,?))) )
			else in %?: OUT
				do ( .., OUT )
-
		else // done pending

		// process data & set parent(=driver) notification
+		on : r : ? // r pushed or popped
			// test if r has other feeders starting at s's starting frame
			in ( (.,%(*s:((.,?),.))), %? ): ~*s
				do >&" *** Error: Yak: rule '%_': "
					"multiple interpretations ***\n": %(%?:((.,?),.))
				do ( .., ERR )	// notify err
				do : s : ~.
		else in ( ?:(.,('[',*f)):~*r, *s ) // s has rule starting this '[' frame
			do : pending : ( IN, %? )
		else in ( ?:(.,(']',*f)):~*r, *s ) // s has rule starting this ']' frame
			in *f: ~(record,*) // output last schema event
				do : pending : ( TAKE, %(*f:(.,?)) )
				do : push : %?
			else do : pending : ( IN, %? )
		else in ( *s, ?:(.,*f)) // this frame is s's last frame
			// cyclic case: test if other feeder starting at s's finishing frame
			in ?: ( (.,%?), *r ): ~*s
				in : push : OUT  // pruning
					do : pending : ( OUT, %? )
					do : push : ~.
				else in ( *s:(((.,~'\0'),.),.), (']',.))
					do : pending : ( TAKE, %(*f:(.,?)) )
					do : pop : %? // take before pop
				else do : pending : ( OUT, %? )
			else in ( *r, base )
				in : push : OUT  // pruning
					do : pending : OUT
					do : push : ~.
				else in ( *s:(((.,~'\0'),.),.), (']',.))
					do : pending : ( TAKE, %(*f:(.,?)) )
					do : pop : OUT
				else do : pending : OUT
			else in ( %(*r,?), ?:((.,%?),.) )
				// set s to the successor of the schema which the current r
				// fed and which started at finishing frame = %(*s,?)
				in : push : OUT  // pruning
					do : pending : ( OUT, %? )
					do : push : ~.
				else in ( *s:(((.,~'\0'),.),.), (']',.))
					do : pending : ( TAKE, %(*f:(.,?)) )
					do : pop : %?
				else do : pending : ( OUT, %? )
			else // no such successor, then we should have had (*r,base)
				do : push : ~.
				do >&" *** Error: Yak: rule '%_': "
					"subscriber has no successor ***\n": %(*r:((.,?),.))
				do ( .., ERR )	// notify err
				do : s : ~.
		else in ?:( *f, . ) // there is a next frame
			// output event, unless *f is a first ']' schema frame
			in *f:(.,?:~EOF):~(record,*):~%(*s:((.,(']',?)),.))
				do : pending : ( TAKE, %? )
			do : f : %?
		else in *f:(.,?:~EOF):~(record,*) // no next frame
			do : pending : ( TAKE, %? )
			do : push : OUT
		else do : state :~.
-

	else in : state : ~.  // next input-traversal cycle - or exit
		on : state : ~.
			// destroy the whole record structure, including rule
			// and schema instances - all in ONE Consensus cycle
			in : record : ~(.,EOF)
				do ~( record )
			else do exit
		else on ~( record )
			// reset: we do not want base rule to catch this event
			do : record : (record,*)
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
			do r~
		do s~
	else in ~.: (r,.) // all r subscribers failed
		do r~
		do s~
	else on ~( .(.,s) ) // feeder rule failed
		do .EXIT
	else in .DONE	// s resp. feeder rule complete
		on ~( .(.,r) ) // successor schema failed
			in ~.: .(.,r) // all successor schemas failed
				do .EXIT // defunct
		on ( ((.,'\0'),.), r ) // r's completion guaranteed
			in ((.,start), r ): ~s // r has other feeder starting at same start frame
				do .EXIT // redundant
		on ~( %(r,?), . ) // r's subscribers connections changed
			in ((.,%(.?)), r ) :~s // r has feeder other than s starting at finish frame
			else in ~.: ( %(r,?), ((.,%(.?)),.) )
				// r has no feeder other than s starting at finish frame, nor has
				// any of r's subscribers a successor starting at finish frame
				do .EXIT // defunct
	else on .( /[[\]]/, . )
		do .DONE
	else in .READY
		on : record : .
			do ~( .READY )
	else in .( ?, s ) // s pending on rule
		on ~.: . < { %%, . }
			in .CYCLIC
				do >&"Warning: Yak: unlocking rule '%_'\n": %(r:((.,?),.))
				do .EXIT
		else	// expecting TAKE from rule schemas
			on ((.,%?), ?:(']',.)) // TAKE: launch successor schema
				do .(((schema, %(*p:(.,?))), %? ), r )
			on ((.,%?), ?:('[',.)) // TAKE: launch successor schema
				do .(((schema, %(*p:(.,?))), %? ), r )
			// sync based on rule schemas
			in (.,%?): ~%(?,CYCLIC): ~%(?,DONE)
				in (.,%?): ~%(?,CYCLIC): ~%(?,DONE): ~%(?,READY)
				else do .READY // all non-cyclic rule schemas ready
			else // all non-cyclic rule schemas complete
				do ( .CYCLIC ? .EXIT : .(.,r) ? .DONE : .EXIT )
	else
		on READY < *s
			do : p : %(*p:(.,?))
			do .READY
		else on ~( TAKE ) < *s
			do .READY
		else on :( TAKE, ? ): ~. < *s
			do .( %<?>, *record ) // TAKE, consumed or unconsumed
		else on :( TAKE, ? ): . < *s
			in ((Rule,?:%<!:(.,?)>),(Schema,~'\0'))
				in ((Rule,%?),(Schema,'\0')) // FORK on null-schema
					do .(((schema, %(*p:(.,?))), (%<?>,*record)), r )
				in ?: ((rule,%?), (%<?>,*record)) // rule already instantiated
					do .( %?, s )
					do .CYCLIC
				else do (((rule,%?), (%<?>,*record)) | {
					(((schema, %((Rule,%?),(Schema,?:~'\0'))), %(%|:(.,?))), %| ),
					.( %|, s ) } )
			else in ((Rule,%<!:(.,?)>),(Schema,'\0'))
				in : p: (.,?)
					do : p : %? // moving on
					do : s : !! Take( ((*,p),%?), ( %<?:'['> ? ((*,event),%(*record:(.,?))) :) )
			else
				do >&"Error: Yak: rule '%_' not found or invalid\n": %<!:(.,?)>
				do .EXIT
		else on exit < *s // FAIL
			do .EXIT

: Take
	on ~( ., %% ) < ..
		do exit
	else in READY
		on : record : ?  < ..
			do : event : %<?:(.,?)>
			do ~( READY )
	else on : event : .
		in : p : '\0'
			do :( TAKE, '[' ): ~. // TAKE, event unconsumed
		else in : p : (?,.) // expected @ current, not terminating position
			in %?: ( %, ? )
				do :( TAKE, '[' ): %?
			else in %?: ( '\\', ? )
				in %?: w
					in : event : /[A-Za-z0-9_]/
						do check
						do TAKE~
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
					do TAKE~
				else
					do : p : %(*p:(.,?))
					do : event : *event // REENTER
			else in : event : %?
				do : p : %(*p:(.,?))
			else do exit // FAIL
		else // *p is a base entity (singleton) other than '\0'
			do >&"Error: Yak: %_-terminated Schema not supported\n": *p
			do exit // FAIL
	else on : p : ?
		in %?: '\0'
			do :( TAKE, ']' ): ~. // TAKE, event consumed
		else in	%?: (( %, ? ), . )
			do :( TAKE, ']' ): %?
		else do READY
	else on ~( TAKE )
		do READY
	else on ( TAKE, . )
		do exit

