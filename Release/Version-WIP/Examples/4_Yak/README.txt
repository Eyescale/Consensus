
	a schema instance is entirely defined as
		.s:(( schema, .position ), .start )
	    where
		position[1] is the entry into s's corresponding Schema definition (literal)
		start (aka. start frame) can be either one of the following
			( [, .record:( ., event ))	// event to be consumed
			( ], .record:( ., event ))	// event already consumed

	a rule instance is entirely defined as
		.r:(( rule, .id ), .start )
	    where
		id is r's corresponding Rule identifier
		start (aka. start frame) can be either one of the following
			( [, .record:( ., event ))	// event to be consumed
			( ], .record:( ., event ))	// event already consumed

	a schema thread is a relationship instance
		:( .s:(( schema, .position ), .frame ), .r )
	    where
		r is pending on s (aka. s "feeds" r)
		<=> s's Schema position[1] is part of r's Rule definition

	rule and schema instances may also possess the following dependencies
		( r, s )		// s is pending on r
		( s, successor )	// successor takes over feeding s's rule,
					// while s, pending on another rule, launches
					// new successor each time that rule completes

	Due to the possibility of cyclic dependencies, events associated with rule and
	schema instances are processed together in the schema thread sub-narrative.
	The schema thread relationship instance carries the following events & conditions:

	    READY	input event processing done, pending on new input event
	    CYCLIC	reached (%,.rule) position and found rule already instantiated
			for the very same start frame
	    DONE	when either one of the following conditions is true
			. reached '\0' position
			. at (%,.rule) position, when all non-cyclic rule schemas are DONE
	    EXIT	caused by either one of the following conditions
			. all r feeder schemas fail, alt. all r subscriber schemas fail
			. feeder rule fail
			. feeder rule complete, and all s's successor schemas failed
			. r has other feeder starting at the same frame, completion guaranteed
			. r has no feeder other than s starting at finish frame, nor has
			  any of r's subscribers a successor starting at finish frame
			. input event test failed

	Note that the only reason for the relationship instances of the type ( r, s ) to be
	dotted - e.g. do .( %|, s ) - is for optimizing e.g. in .( ., s )

	The difference between yak.proper and yak.story is that the latter uses the schema
	thread instead of the schema instance to establish connections, e.g.
		do .( ((schema,position),frame), r )		// yak.story
	    vs. do ( ((schema,position),frame) | ( %|,r) )	// yak.proper

	Again, for optimization purpose.


Notes
1. a Schema position, being sub-literal, may be shared among multiple Schemas


