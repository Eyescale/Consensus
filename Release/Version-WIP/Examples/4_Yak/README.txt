Name
	Consensus/Release/Version-WIP/Examples/4_Yak/yak.story

Usage
	../../B% -f Schemes/scheme-file yak.story

Entities
	a schema instance is entirely defined as
		.s:(( schema, .position ), .start )
	    where
		.position is the entry into s's corresponding Schema definition (literal[1])
		.start (aka. start frame) can be either one of the following
			( [, .record:( ., event ))	// event to be consumed
			( ], .record:( ., event ))	// event already consumed

	a rule instance is entirely defined as
		.r:(( rule, .id ), .start )
	    where
		.id is r's corresponding Rule identifier
		.start (aka. start frame) can be either one of the following
			( [, .record:( ., event ))	// event to be consumed
			( ], .record:( ., event ))	// event already consumed

	a schema thread is a relationship instance
		:( .s:(( schema, .position ), .frame ), .r )
	    where
		.r is pending on .s (aka. .s "feeds" .r)
		=> .s's Schema position[1] is part of .r's Rule definition

	rule and schema instances may also possess the following dependencies
		( r, s )		// s is pending on r
		( s, successor )	// successor takes over feeding s's rule,
					// while s, pending on another rule, launches
					// new successor each time that rule completes

	Due to the possibility of cyclic dependencies, events associated with rule and
	schema instances are handled together in the schema thread sub-narrative, the
	schema thread relationship instance carrying the following conditions:

	    READY	input event processing done, pending on new input event
	    CYCLIC	reached (%,rule) position and found rule already instantiated
			for the very same start frame
	    DONE	when either one of the following conditions is true
			. reached '\0' position
			. at (%,rule) position, when all non-cyclic rule schemas are DONE
	    EXIT	See Schema Thread Exit Condition section below

	Note that the only reason for the relationship instances of the type ( r, s ) to be
	dotted - e.g. do .( %|, s ) - is for optimizing e.g. in .( ., s )

	The difference between yak.proper and yak.story is that, for optimization purpose,
	the latter uses the schema thread instead of the schema instance to establish
	connections, e.g.
		do .( ((schema,position),frame), r )		// yak.story
	   vs.  do ( ((schema,position),frame) | ( %|,r) )	// yak.proper

Behavior
	The story alternates between

		: IN state - IN stands for INPUT phase
		: OUT state - OUT stands for OUTPUT phase

	During the INPUT phase, rule and schema instances are created and released
	(see Schema Thread Exit Conditions section below) based on user input and the
	(( Rule,.),(Schema,.)) scheme definition, resulting in the following schema
	thread organization:

		r4				     .====.
		r3		       .=====.       |    |
		r2		.=====(       )=====(      )====.
		base	.======(                                 )========.

		record  . . . . . . . . . . . . . . . . . . . . . . . . . .

	  where
		.=====. and )=====. are terminated, meaning the corresponding s
		schema thread verifies ( s, f ) where f is its finish frame.

		.=====( and )=====( are unterminated (pending on rule), but the
		corresponding s schema thread entity verifies ( s, s' ) where s'
		is s's successor schema thread.

	Note that during that phase we may have not just one, but a number of these,
	starting e.g.  from base at initial frame ( ']', (record,*))

	During the OUTPUT phase, the next rule is pushed if and only if it has
	a schema thread starting & not finishing at the same, current frame.

	Otherwise, that is, if the rule starting this frame only has threads
	starting & finishing at the same, current frame, then we pop the current
	rule: this corresponds to the case where the current schema thread
	*at its last position* subscribed to a rule including null-schema, all the
	others failing, leading the current schema thread to terminate.

Schema Thread Exit Conditions

	Schema thread s are released on either one of the following conditions
	(r being the rule s feeds)
	
	    1. all r feeder schemas fail, alt. all r subscriber schemas fail

	    [ if s is DONE and was pending on rule ]

	    2. feeder rule complete - that is: all its non-cyclic schema
	       threads are DONE - and all s's successor schemas failed

	    [ else if s is DONE and terminated ]

	    3. none of r's subscribers has a successor starting at s's finish
	       frame - and r has no feeder other than s starting at that frame

	    4. s feeds base and another base-feeding schema thread terminates

	    [ endif ]

	    5. s's feeder rule failed

	    6. s Take fails


Notes
1. a Schema position, being sub-literal, may be shared among multiple Schemas

