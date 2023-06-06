/*===========================================================================
|
|				yak story
|
+==========================================================================*/
/*
	Usage
		../../B% -f Schemes/file yak.story < input
	 or
		as driver interface - see yak-drive.story
*/
#ifdef YAK_DRIVE
#include "yak.bm"
#else
// #define DEFAULT
// #define DEBUG
:
	on init
		// base rule definition must exist and have non-null schema
		in (( Rule, base ), ( Schema, ~'\0' ))
			do :< record, %% >:< (record,*), IN >
#ifdef DEFAULT
		else
			// default Scheme: dot-terminated number
			do {
				(( Rule, base ), ( Schema, (:%term:)))
				(( Rule, term ), ( Schema, (:%int.:)))
				(( Rule, int ), ( Schema, {
					(:\d:), (:%int\d:) } )) }
			do :< record, %% >:< (record,*), IN >
#else
		else
			do >"Error: Yak: base rule not found or invalid\n"
			do exit
#endif

	//--------------------------------------------------
	//	INPUT
	//--------------------------------------------------
	else in : IN
		on : . // start base rule instance - feeding base, also as subscriber schema
			do (((rule,base), (']',*record)) | {
				(((schema, %((Rule,base),(Schema,?:~'\0'))), %(%|:(.,?))), %| ),
				.( %|, base ) } )
		else in .( ?, base )
			%( . )
			in ~.: .READY // sync based on rule schemas
				in (.,%?): ~%(?,DONE)
					in (.,%?): ~%(?,DONE): ~%(?,READY)
					else do .READY // all feeder schemas ready
				else on ~.: . < { ~(..), %% }
					in ((.,%?),(']',(record,*)))
						do ~( %?, base )
					else in ((.,%?),('[',((record,*),.)))
						do ~( %?, base )
					else in ~.: ((.,%?),(/[[\]]/,.))
						do ~( %?, base )
					else
						do : OUT
			else on .READY
				in : carry : ?
#ifdef DEBUG
					do >"---------------- carry: %_\n": %?
#endif
					do : input : %?
					do : carry : ~.
				else
					do input:"%c"<
			else on : input : ?
#ifdef DEBUG
				do >"---------------- input: %_\n": %?
#endif
				// could do some preprocessing here
				do : record : (*record,%?)
			else on : input : ~.
				do : record : (*record,EOF)
			else on : record : .
				do ~( .READY )
		else on ~( ., base ) // FAIL
#ifdef DEBUG
			do >"---------------- FAILED\n"
#endif
			in (((record,*),.),.) // not first input
				do : record : %(*record:(?,.))
				do : carry : %(*record:(.,?))
			do : OUT

	//--------------------------------------------------
	//	OUTPUT
	//--------------------------------------------------
	else in : OUT
		.s .f .r
		on : .
			in : record : (record,*)
				do >"(nop)\n"
				do exit
			else
				do :< s, f >:< base, (record,*) >
				in : carry : . // trim record
					do ~( *record, . )

		else on : pop : ? // popping argument = s's finishing frame, or OUT
			in %?: OUT
				in *f: (.,?:~EOF): ~(record,*)
					do >"%s": %?
				do : ~.
			else
				// output current schema's last event, if consumed
				in ( *s:(((.,~'\0'),.),.), (']',?:~(record,*)) )
					do >"%s}": %(%?:(.,?))
				else do >"}"
				in ~.: ( *r, base )
					/* set s to the successor of the schema which the current r
					   fed and which started at s's finishing frame */
					in ( %(*r,?), ?:(((schema,.),%?),.) )
						do :< s, r >:< %?, %(%?:(.,?)) >
					else // no such successor, we should have (*r,base)
						do >" *** Error: Yak: rule '%_': "
							"subscriber has no successor ***\n": %(*r:((.,?),.))
						do : s : ~.
				else // back to base
					in %?: ('[',.) // completed unconsumed
						in : f : (.,?:~EOF)
							do : carry : %?
						else do >"\n" // EOF
					else in ( *f, ?:~EOF )
						// right-recursive case: completes on failing next frame
						do : carry : %?
					do : ~.
			do : pop : ~.

		else on : r : ? // r pushed or popped
			/*---------------------------------------------------------------
			// test if r has other feeders starting at s's starting frame
			in ( (.,%(*s:((.,?),.))), %? ): ~*s
				do >" *** Warning: Yak: rule '%_': "
					"multiple interpretations ***\n": %(%?:((.,?),.))
			-----------------------------------------------------------------*/
			on : pop : ~.
			else // output r begin
				do >"%%%s:{": %(%?:((.,?),.))

		// s has rule with event to-be-consumed starting this frame => pushing or popping
		else in ( (.,?:('[',*f)):~*r, *s )
			// rule has schema starting & not finishing at the same frame => pushing
			in ?: (((.,~'\0'), %?), %(?:(.,%?):~*r,*s)): ~%(?,%?)
				do :< s, r >:< %?, %(%?:(.,?)) >
			// s has sibling starting and not finishing at s's finishing frame
			else in ?: ((.,%?), *r ): ~%(?,%?): ~*s 
				do : s : %?
			else do : pop : %?

		// s has rule with event unconsumed starting this frame => pushing or popping
		else in ( (.,?:(']',*f)):~*r, *s )
			// rule has schema starting & not finishing at the same frame => pushing
			in ?: (((.,~'\0'), %?), %(?:(.,%?):~*r,*s)): ~%(?,%?): ~%(?,('[',(*f,.)))
				// output current schema's last event, providing it's not also starting
				in *f: ~%(*s:((.,(']',?)),.)): ~(record,*)
					do >"%s": %(*f:(.,?))
				do :< s, r >:< %?, %(%?:(.,?)) >
			// s has sibling starting and not finishing at s's finishing frame
			else in ?: ((.,%?), *r ): ~%(?,%?): ~*s 
				// output current schema's last event, providing it's not also starting
				in *f: ~%(*s:((.,(']',?)),.)): ~(record,*)
					do >"%s": %(*f:(.,?))
				do : s : %?
			// s has sibling starting and not finishing at s's post-finishing frame, unconsumed
			else in ?: ((.,('[',(*f,.))), *r ): ~%(?,('[',(*f,.))): ~*s 
				// output current schema's last event, providing it's not also starting
				in *f: ~%(*s:((.,(']',?)),.)): ~(record,*)
					do >"%s": %(*f:(.,?))
				do : s : %?
			else do : pop : %?

		else in ( *s, ?:(.,*f)) // this frame is s's last frame => popping
			// s has sibling starting and not finishing at s's finishing frame
			in ?: ((.,%?), *r ): ~%(?,%?): ~*s 
				// output current schema's last event, if consumed
				in ( *s:(((.,~'\0'),.),.), (']',?:~(record,*)) )
					do >"%s": %(%?:(.,?))
				do : s : %?
			else do : pop : %?

		else // moving on
			in ?:( *f, . ) // there is a next frame
				// output event, unless *f is a first ']' schema frame
				in *f: (.,?): ~(record,*): ~%(*s:((.,(']',?)),.))
					do >"%s": %(*f:(.,?))
				do : f : %?
			else
				do : pop : OUT

	//--------------------------------------------------
	//	TRANSITION
	//--------------------------------------------------
	else on : ~.
		// destroy the whole record structure, including rule
		// and schema instances - all in ONE Consensus cycle
		in : record : ~(.,EOF)
			do ~( record )
		else do exit

	else on ~( record ) // next input-traversal cycle
		// reset: we do not want base rule to catch this frame
		do :< record, %% >:< (record,*), IN >

#endif // YAK_DRIVE

/*===========================================================================
|
|	yak.story input schema threads sub-narrative definition
|
+==========================================================================*/
// #define MEMOPT

.s: ((( schema, .p ), .f ), .r )
	on .EXIT
		in ~.: (.,r): ~%(?,EXIT) // all r feeder schemas failed
			do ~( r )
		else do ~( s )
#ifdef MEMOPT
	else in ~.: (r,.) // all r subscribers failed
		do ~( r )
#endif
	else
	//--------------------------------------------------
	//	s PENDING ON RULE
	//--------------------------------------------------
+	.q
	in .( ?, s ) // s is pending on rule
		on ~.: .
			do >&"Warning: Yak: unlocking rule '%_'\n": %(r:((.,?),.))
			do .EXIT
		else in .DONE
			on ~( .(.,r) )
				in .(.,r) // s still has successor
				else do .EXIT // defunct
		else in .READY
			on : record : .
				do ~( .READY )
		else	// expecting TAKE from rule schemas
			on ((.,%?), ?:('[',.)) // TAKE: launch successor schema(s)
				do .(((schema, %(*q:(.,?))), %? ), r )
				on ((.,%?), ?:(']',.)) // TAKE: launch successor schema
					do .(((schema, %(*q:(.,?))), %? ), r )
			else on ((.,%?), ?:(']',.)) // TAKE: launch successor schema
				do .(((schema, %(*q:(.,?))), %? ), r )
			else // sync based on rule schemas
				in (.,%?): ~%(?,CYCLIC): ~%(?,DONE)
					in (.,%?): ~%(?,CYCLIC): ~%(?,DONE): ~%(?,READY)
					else do .READY // all non-cyclic rule schemas ready
				else // all non-cyclic rule schemas complete or non-existing
					do ( .CYCLIC ? .EXIT : .(.,r) ? .DONE : .EXIT )

	else on ~( .(.,s) ) // feeder rule failed
		do .EXIT
	//--------------------------------------------------
	//	s TERMINATED
	//--------------------------------------------------
	else in .DONE
#ifdef MEMOPT
		on (((.,'\0'),.),r:%((.,f),?)) // r's completion guaranteed and
			do .EXIT // r has other feeder started at same start frame
		else on ~( %(r,?), . ) // r's subscribers connections changed
#else
		on ~( %(r,?), . ) // r's subscribers connections changed
#endif
			in ( %(r,?), ((.,%(.?)),.) )
			else // none of r's subscribers has a successor starting at s's finish frame
				in ((.,%(.?)), r ) :~s
				else // r has no feeder other than s starting at s's finish frame
					do .EXIT // defunct
		else on ( (r,base) ? ( (?,r):~%(?,(.,r)):~s, DONE ) :)
			do .EXIT // redundant

	else on .( /[[\]]/, . ) // s terminated
		do .DONE
	else
	//--------------------------------------------------
	//	s PENDING ON EVENTS
	//--------------------------------------------------
+	.event
	in .READY
		on : record : (.,?)
			do ~( .READY )
			do : event : %?
	else on : event : .
		in : q : '\0'
			do .( '[', *record ) // terminate, unconsumed
		else in : q : (?,.) // expected @ current, not terminating position
			in %?: ( %, ? )
				in (( Rule, %? ), ( Schema, ~'\0' ))
					in ?: ((rule,%?), ('[',*record)) // rule already instantiated
						do .( %?, s )
						do ( %?:r ? .CYCLIC :)
					else do ( ((rule,%?), ('[',*record)) | {
						(((schema, %((Rule,%?),(Schema,?))), %(%|:(.,?))), %| ),
						.( %|, s ) } )
				else
					do >"Error: Yak: rule '%_' not found or invalid\n": %?
					do .EXIT
			else in %?: ( '\\', ? )
				in %?: i
					in : event : /[0-9]/
						do .check
						do .READY
					else in .check
						do ~( .check )
						do : event : *event // REENTER
						do : q : %(*q:(.,?))
					else do .EXIT // FAIL
				else in %?: d
					in : event : /[0-9]/
						do : q : %(*q:(.,?))
					else do .EXIT // FAIL
				else in %?: w
					in : event : /[A-Za-z0-9_]/
						do .check
						do .READY
					else in .check
						do ~( .check )
						do : event : *event // REENTER
						do : q : %(*q:(.,?))
					else do .EXIT // FAIL
				else in %?: 0
					in : event : '\0'
						do : q : %(*q:(.,?))
					else do .EXIT // FAIL
				else in : event : %?
					do : q : %(*q:(.,?))
				else do .EXIT // FAIL
			else in %?: ' '
				in : event : /[ \t]/
					do .READY
				else
					do : event : *event // REENTER
					do : q : %(*q:(.,?))
			else in : event : %?
				do : q : %(*q:(.,?))
			else do .EXIT // FAIL
		else // *q is a base entity (singleton) other than '\0'
			do >"Error: Yak: %_-terminated schema not supported\n": *q
			do .EXIT // FAIL
	else on : q : ?
		in %?: '\0'
			do .( ']', *record ) // terminate, consumed
		else in %?: (( %, ? ), . )
			in (( Rule, %? ), ( Schema, ~'\0' ))
				in ?: ((rule,%?), (']',*record)) // rule already instantiated
					do .( %?, s )
					do ( %?:r ? .CYCLIC :)
				else do ( ((rule,%?), (']',*record)) | {
					(((schema, %((Rule,%?),(Schema,?))), %(%|:(.,?))), %| ),
					.( %|, s ) } )
			else
				do >"Error: Yak: rule '%_' not found or invalid\n": %?
				do .EXIT
		else do .READY
	else
	//--------------------------------------------------
	//	s INIT
	//--------------------------------------------------
+	on ( s )
		// schema is in null position AND has predecessor
		in ( p:'\0' ? ((.,r), s ) :)
			do .( f ) // TAKE as-is
		else
			do : q : p
			in f: ('[',(.,?)) // event to be consumed
				do : event : %?

