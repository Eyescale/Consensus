Usage
	./B% -i

Description
	Our goal is to allow the user to, interactively

	. specify systems simply in terms of cosystems and occurrences,
	  where cosystem occurrences = events, conditions and actions

	. navigate their system status and dependencies both statically
	  and dynamically, that is: when the system is in action

Requirement Specification
	Requirement Specifications can be reduced to short statements, which
	which we call occurrences, describing the System's conditions, events,
	and actions, according to the syntax

		in occurrence(s) // condition
			on occurrence(s) // event
				do occurrence(s) // action

	The System's Requirements Specifications process involves identifying,
	for each occurrence
		. the referent, i.e. the cosystem for which this occurrence
		  represents a condition ON|OFF
		. the ON|OFF set of triggers, each trigger composed of
			its triggering set of occurrences ON|OFF (events)
			its enabling set of guards, each guard composed of
				its defining set of occurrences ON|OFF (conditions)

Data Model
	We use the following Data Model when the System is in execution,

			         .------------- Guard
			         v                ^
			 .-------+----- Trigger   |
			 v		   ^      |
	              Action               |      |
		         |----------------- 	  |
			  ------------------------
	where
		Action: ((occurrence,ON|OFF),cosystem)

	. Occurrences are statements representing cosystem conditions
	. Each occurrence actually doubles as (occurrence,{ON,OFF}) to allow 
	  the user to specify negated conditions, and is paired with cosystem
	  to allow the same statement to originate from multiple cosystems

	hence
		Occurrence:	%(?,ON|OFF)
		Cosystem:	%((.,ON|OFF),?)
		Action:		%(.,?:((.,ON|OFF),.))
		Trigger:	%(?,((.,ON|OFF),.))
		Guard:		%(?,(.,((.,ON|OFF),.)))
		Event:		%(?:((.,ON|OFF),.),%(?,((.,ON|OFF),.)))
		Condition:	%(?:((.,ON|OFF),.),%(?,(.,((.,ON|OFF),.))))

Execution Model
	Once launched, we want our System to execute the following, which
	we call System frame, over and over again:

	        for each action
	            done = 0
	            for each action<-trigger barring done==1
	                if all trigger events are verified
	                    for each guard in trigger<-guard barring done==1
	                        if all guard conditions are verified
	                            perform action
	                            done = 1
	                        end if
	                    end for
	                end if
	            end for
	        end for


	In Consensus terms, as in reality, all cosystems run in parallel,
	and we want all actions to also (at least conceptually) execute in
	parallel, all in one frame.

	Furthermore, we do not want each cosystem to keep track of all the other
	cosystems conditions - although we could - but rather: each cosystem will
	keep & update each of its own action<-trigger<-guard.status according
	to system events.

	: Cosystem
		per this->action:[ occurrence, ON|OFF ] such that
		    there is one guard among action<-trigger<-guard verifying
		    . guard.status is clear, AND
		    . there is one trigger among guard<-trigger for which
		      all trigger<-event are occurring

				execute action // do : occurrence : ON|OFF

				remove action's corresponding condition from all
				action<-guard.status // for next frame

				add complementary condition to all action<-guard.status
				for which this complementary condition is required,
				where complementary := (( occurrence, ~ON|OFF ), %% )

		per : ? : . < .	// update next frame's conditions
			remove EENO's corresponding condition from all
			action<-guard.status [EENO is condition's notification]

			add complementary condition to all action<-guard.status
			for which this complementary condition is required,
			where complementary := (( %<?>, ~%<!:(.,?)> ), %< )

Performance
	The expression ~(.,(?,Status)) does not yield any pivot as it is
	negated.

	Tagging guards CLEAR upon their last condition being removed will
	allow us to use the CLEAR guards list as a pivot, which not only
	benefits performances but also is consistent with the cellular
	automata execution principle:
		in state*
			on event
				do action

	Further performance consideration motivate our shared arena [_]
	usage and '&' data access by reference, which are described in
	the New Features section

	*whereby a cosystem's "current" state is defined as its set of
	CLEAR guard conditions

Implementation
	Each cosystem implements the following narrative - that is: each
	cosystem is effectively a class of its own, subclass of Cosystem

	using
		|^list		adds current entity to list
		|^list~		removes current entity from list
		.%list~ {_}	flushes list while performing expression, in
				which ^. references the current list element

: Cosystem
	en %(*?:ON) // enable cosystem occurrence's sub-narratives
	on init
		// initialize ( &condition, ( Status, &guard )), and
		// enrol condition-free guards into %guard list
		per [( ?, ( ., ((.,ON|OFF),%(*?:%%)) ))]
			do ( [(.,%<?>)] ? // guard has condition(s)
				( &%[(?,%<?>)], ( Status, &%<?> )) :
				( &%<?>|^guard ) )
	else
		// flush %tag list & enrol condition-free guards, pre-frame
		.%tag~ { ^.:~%(.,(Status,?))|^guard }

		// see the New Features section for EEVA definition
		per [( %guard, ( ~%(~EEVA,?), (?,%(*?:%%)) ))]

			// execute action - assuming %<?:( occurrence, ON|OFF )>
			do : &%<?:(?,.)> : %<?:(.,?)>

			// remove action-corresponding condition from relevant guard
			// Status, and tag these guards
			in [ ?:( %<?>, %(*?:%%) ) ] // action-corresponding condition
				do ~( &%<?>, %( Status, .|^tag ))

			// add action-complementary condition to relevant guard
			// Status, and decommission these guards
			in [ ?:(( %<?:(?,.)>, ~%<?:(.,?)> ), %(*?:%%)) ]
				do ( &%<?>, %( Status, &%[(%<?>,?)]|^guard~ ))

		// update all guard status from EENO - for next frame
		per : ? : . < .
			// remove eeno-corresponding condition from relevant guard
			// Status, and tag these guards
			in [ ?:(( %<?>, %<!:(.,?)> ), %(*?:%<)) ]
				do ~( &%<?>, %( Status, .|^tag ))

			// add eeno-complementary condition to relevant guard
			// Status, and decommission these guards
			in [ ?:(( %<?>, ~%<!:(.,?)> ), %(*?:%<)) ]
				do ( &%<?>, %( Status, &%[(%<?>,?)]|^guard~ ))

: cosystemA : Cosystem
	...
:&[ "occurrence" ]
	...
:&[ "occurrence" ]
	...
:&[ "occurrence" ]
	...

: cosystemB : Cosystem
	...

...

	Notes
	1. guards matching %( Status, ? ) refer to local actions - no check needed
	2. guards with no condition have no Status - but are enrolled permanently
	3. if a trigger has no event ~%(~EEVA,?) always passes - as intended
	4. B% current implementation will issue warning if second term is void in
		do ( &%<?>, %( Status, &[(%<?>,?)]|^guard~ ))

New Features
	1. !! Unnamed Base Entities
    	2. %identifier list variables
	3. [_] shared arena - aka. partition
	4. & access by reference
    	5. <<_>> EENO Condition (EENOC) and EEVA

    1. !! Unnamed Base Entities
	To spare us the hassle of managing our own pool of free Trigger and
	Guard entities, we want the capability to allocate each Trigger and
	Guard instance as unnamed base entity.

	Allocation
		Unnamed base entities are allocated using the operator NEW (!!)
		as standalone, possibly followed by | and relevant %| relationship
		instances, e.g.
			do : trigger : !!
			do : trigger : !! | (%|,(*action,Trigger))
			do !! | { ((%|,field),value), ((%|,f),v), ((%|,f),v), ... }

		Note that if not coupled by the end of the do operation - e.g.
			do !! | toto
		the newly allocated entity will be instantly and quietly removed.

	Usage
		In the last example above, the record verifying [ field, value ]
		and [ f, v ] conditions is given by
			%((?,field),value):%((?,f),v):%((?,f),v)

		The "field" value of the record verifying [f,v] conditions is
		therefore given by
			%(( %((?,f),v):%((?,f),v), field ), ? )

		Note that we could also introduce the operator ^ so that
			expression^(?,.) resp. (.,?) = corresponding sub
			expression^(!,.) resp. (.,!) = corresponding as_sub

		This would allow the "field" value of the record verifying [f,v]
		conditions to be given by
			%(((.,field),?)^((?,.),.):( %((?,f),v):%((?,f),v) ))

	Release
		Either directly by user or automatically when left dangling - in
		which case no release event is issued

    2. %identifier list variables

	|^list		adds current entity to list
	|^list~		removes current entity from list
	.%list~ {_}	flushes list while performing expression, in
			which ^. references the current list element

	Note that pre-frame execution is guaranteed for
		.%tag~ { ^.:~%(.,(?,Status))|^guard }

    3. [_] shared arena - aka. partition

	[_] signifies "in the shared arena" - data shared across System
	Note: [_,_] is not allowed => must use [(_,_)]

	%[_] matching entities, when marked, are accessed via %<_>

    4. & access by reference

	'&' in &%<_> and &%[_] resp. &[_] specifies "as-is" - i.e.
	accessed by reference

    5. <<_>> EENO Condition (EENOC) and EEVA
	In the code above, EEVA represents the following EENO Condition

		<< : &[^((?,.),.)] :^((.,?),.) < [*^(.,?)] >>

	Where
		1. << event < src >> passes iff event < src is verified

		2. The symbol '^' represents the entity corresponding to
		the current expression term, e.g.

			<< : ^((?,.),.) : ^((.,?),.) < ^(.,?) >>

		expresses that, in order to pass, the current expression
		term must match an external event in the form
			: a : b < c
		such that 
			a: ^((?,.),.)	// current.sub[0].sub[0]
			b: ^((.,?),.)	// current.sub[0].sub[1]
			c: ^(.,?)	// current.sub[1]

	Note that we use the syntax ^((?,.),.) instead of %(%.:((?,.),.)
	as %. representing the expression's current term could hold more
	than one entity, and therefore %(%.:((?,.),.)) and %(%.:((.,?),.))
	might not relate to the same entity

Task List
	./B% -i
	> support do expression // including output
	> allow to create System: ON|OFF relationship instances
		need support for unnamed entity
		need support for do "occurrence"
		want to use %system / do { ON|^system, OFF|^system }
	> allow	to output System definition [to stdout and/or file]

		: cosystem : Cosystem
		:&[ "occurrence" ]
			ON|OFF
				: "occurrence" : ON|OFF < cosystem
				...
			/
				: "occurrence" : ON|OFF < cosystem
		:&[ "occurrence" ]
			ON|OFF
				: "occurrence" : ON|OFF < cosystem
			/
				: "occurrence" : ON|OFF < cosystem
		etc.

		: cosystem : Cosystem
		etc.

	  Note that the above represents the overall System definition,
	  aka. data structure, and not the cosystem sub-class definitions,
	  aka. narratives and sub-narratives, which are kept in separate
	  cosystem.bm files using the same template

		: cosystem : Cosystem
			...
		:&[ "occurrence" ]	// invoked in/on : this : ON
			...
		:&[ "occurrence" ]	// invoked in/on : this : ON
			...
		:&[ "occurrence" ]	// invoked in/on : this : ON
			...

	  but where
		... represents user B% code - possibly including
		do : this : OFF

	> allow to launch system
			>>>>> launch from files <<<<<

		=> must be able to
			load system definition from file
			load Cosystem class definition from file
			load each cosystem.bm sub-class definition from file

		need support for sub-class execution
			need support for shared memory access
			need support for access by address
			need support for EENOC
			need support for %list

	Eventually also support:
	./B% -launch path/system.bm

