/*===========================================================================
|
|				yak story
|
+==========================================================================*/
/*
   Usage
	../../B% -f Schemes/file yak.story < input
   Purpose
	general Scheme testing
*/
// #define DEBUG
#define PROMPT
:
	on init
		// base rule definition must exist and have non-null schema
		in (( Rule, base ), ( Schema, ~'\0' ))
			do : record : (record,*)
			do : IN
		else
			do >"Error: Yak: base rule not found or invalid\n"
			do exit
	else in : IN
		on : . // start base rule instance - feeding base also as subscriber schema
			do (((rule,base), (']',*record)) | {
				(((schema, %((Rule,base),(Schema,?:~'\0'))), %|:(.,?)), %| ),
				.( %|, base ) } )
		else in .( ?, base )
			%( . )
			in ~.: .READY // sync based on rule schemas
				in (.,%?): ~%(?,DONE)
					in (.,%?): ~%(?,DONE): ~%(?,READY)
					else do .READY // all feeder schemas ready
				else on ~.: .
					in ((.,%?),(']',(record,*)))
						do ~( %?, base )
					else in ((.,%?),('[',((record,*),.)))
						do ~( %?, base )
					else in ~.: ((.,%?),(/[[\]]/,.))
						do ~( %?, base )
					else do : OUT
			else on .READY
				in : carry : ?
#ifdef DEBUG
					do >"---------------- carry: %_\n": %?
#endif
					do : input : %?
					do : carry : ~.
				else
#ifdef PROMPT
					in ~.: : input : ~'\n'
						do >&"yak > "
#endif
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
		else on : pop : ? // %? = s's finishing frame, or OUT
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

		else in ?:( *f, . ) // there is a next frame
			// output event, unless *f is a first ']' schema frame
			in *f: (.,?): ~(record,*): ~%(*s:((.,(']',?)),.))
				do >"%s": %(*f:(.,?))
			do : f : %?
		else
			do : pop : OUT
	else
		on : ~.
			// destroy the whole record structure, including rule
			// and schema instances - all in ONE Consensus cycle
			in : record : ~(.,EOF)
				do ~( record )
			else
				do exit
#ifdef PROMPT
				do >&"  \n" // wipe out ^D
#endif
		else on ~( record ) // next input-traversal cycle
			// reset: we do not want base rule to catch this frame
			do :< record, %% >:< (record,*), IN >

/*---------------------------------------------------------------------------
|
|	yak input schema threads sub-narrative definition
|
+--------------------------------------------------------------------------*/
#include "yak-take.bm"

