About
	Our goal is to allow the user to, interactively

	. specify systems simply in terms of cosystems and occurrences,
	  where cosystem occurrences = events, conditions and actions

	. navigate their system status and dependencies both statically
	  and dynamically, that is: when the system is in action

	The challenge will be the interaction design

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
		. the ON set of triggers, each trigger composed of
			its triggering set of occurrences ON|OFF (events)
			its enabling set of guards, each guard composed of
				its defining set of occurrences ON|OFF (conditions)
		. the OFF set of triggers, each trigger composed of
			its triggering set of occurrences ON|OFF (events)
			its enabling set of guards, each guard composed of
				its defining set of occurrences ON|OFF (conditions)

Data Model
	System
	    maps
		Occurrence	Referent	      ON	      OFF
		occurrence	cosystem	trigger	guard	trigger	guard
							guard		guard
							...		...
						...		...
		...		...
	    where
		trigger: { [ [ occurrence, ON|OFF ], cosystem ] }
		guard:	 { [ [ occurrence, ON|OFF ], cosystem ] }

	Cosystem
	    maps
		Action	Occurrence	Type	Trigger	Guard	Status
		action	occurrenceUID 	ON|OFF	trigger	guard	status
							guard	status
							...	...
						...
			...
		...
	    where
		trigger: { [ [ occurrenceUID, ON|OFF ], proxy ] }
		guard:   { [ [ occurrenceUID, ON|OFF ], proxy ] }

Execution Model
	Once launched, we want our System to execute the following, which
	we call System frame, over and over again:

	        for each action
	            done = 0
	            for each action->trigger barring done==1
	                if all trigger events are verified
	                    for each guard in trigger->guard barring done==1
	                        if all gard conditions are verified
	                            perform action
	                            done = 1
	                        end if
	                    end for
	                end if
	            end for
	        end for
				
	In Consensus terms, all cosystems run in parallel, and we want all actions
	to also (at least conceptually) execute in parallel, all in one frame.

	Furthermore, we do not want each cosystem to keep track of all the other
	cosystems conditions (although we could), but rather: each cosystem will
	keep & update each of its own action->trigger->guard.status according
	to system events.

	: Cosystem
		per this->action:[ occurrence, ON|OFF ] such that
		    there is one guard among action->trigger->guard verifying
		    . guard.status is clear, AND
		    . there is one trigger among guard->trigger for which
		      all trigger->event are occurring

				execute action // do : occurrence : ON|OFF

				remove action's corresponding condition from all
				action->guard.status // for next frame

				add complementary condition to all action->guard.status
				for which this complementary condition is required,
				where complementary := [ [ occurrence, ~ON|OFF ], %% ]

		per : ? : . < .	// update next frame's conditions
			remove EENO's corresponding condition from all
			action->guard.status [EENO is condition's notification]

			add complementary condition to all action->guard.status
			for which this complementary condition is required,
			where complementary := [ [ %<?>, ~%<!:(.,?)> ], %< ]

Implementation
	We use the keywords Action, Trigger, Guard and Status so that
		%( ?, Action ) is the list of actions in current CNDB
		%( ?, ( %?, Trigger ) ) is the list of triggers of the action %?
			%( ?, %? ) is the list of events of the trigger %?
		%( ?, ( %?, Guard ) ) is the list of guards of the trigger %?
			%( ?, %? ) is the list of conditions of the guard %?
			%( ?, ( %?, Status ) ) is the list of conditions of the guard %?'s status

		Note that this convention relies on neither action nor trigger nor 
		guard being proxy - otherwise we should use the same convention as per
			(( *, variable ), value )
	Also
		%<.> is the built-in conditional External Event Variable Assignment (EEVA)
		     %<< : ^((?,.),.) : ^((.,?),.) < ^(.,?) >>

	: Cosystem
		on init
			...
		else
			// enable actions based on guard->status and trigger->event EEVA
			%( %( .:~%(.,(?,Status)), ( ?:~%(~%<.>,?), Guard )), ( ?, Trigger ))
			//    ^---- guard           ^---- trigger   action ----^

			// update all guard status from EENO - for next frame
			per : ? : . < .
				// remove condition from all guard.status
				in ?: (( %<?>, %<!:(.,?)> ), %< )
					do ~( %?, ( ., Status ))

				// add complementary condition to the status of the guards
				// for which it is a condition
				in ?: (( %<?>, (%<!:(.,ON)>?OFF:ON) ), %< )
					do ( %?, ( %(%?,?):%(?,(.,Guard)), Status ))
					//--- guard ----^       ^---- trigger

	.action: %( ?, Action )
		// execute action - assuming action:[ occurrence, ON|OFF ]
		do : %(action:(?,.)): %(action:(.,?))

		// remove action's corresponding condition from all guard.status
		in ?: ( action, %% ) // %? is condition
			do ~( %?, ( ., Status ))

		// add complementary condition to the status of the guards
		// for which it is a condition
		in ?: (( %(action:(?,.)), (action:(.,ON)?OFF:ON) ), %% )
			do ( %?, ( %(%?,?):%(?,(.,Guard)), Status ))
			//--- guard ----^       ^---- trigger

New Features
	1. !! Unnamed Base Entities
    	2. %<.> External Event Variable Assignment (EEVA)
    	2. %identifier list variables [Optimization]

    1. !! Unnamed Base Entities
	To spare us the hassle of managing our own pool of free Trigger and
	Guard entities, we shall allocate each Trigger or Guard instance as
	unnamed base entity.

	Allocation
		Unnamed base entities are allocated using the operator NEW (!!)
		possibly followed by | and relevant %| relationship instances
		Examples
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

    2. %<.> External Event Variable Assignment (EEVA)
	We shall implement %<.> to represent the conditional EENOV

		%<< : ^((?,.),.) : ^((.,?),.) < ^(.,?) >>

	Where
		A conditional EENOV (CEENOV) is an EENOV in the form
			%<< event < src >>
		such that
			%<< event < src >> passes iff the EENO
				event < src
			is verified

	The symbol '^' when used inside the CEENOV represents the entity
	corresponding to the current expression term, e.g. the CEENOV

		%<< : ^((?,.),.) : ^((.,?),.) < ^(.,?) >>

	expresses that, in order to pass, the current expression term
	must match an external event in the form
		 : a : b < c
	such that 
		^((?,.),.): a	// current.sub[0].sub[0]
		^((.,?),.): b	// current.sub[0].sub[1]
		^(.,?): c	// current.sub[1]

	Note that we use the syntax ^((?,.),.) instead of %(%.:((?,.),.)
	as %. representing the expression's current term could hold more
	than one entity, and therefore %(%.:((?,.),.)) and %(%.:((.,?),.))
	might not relate to the same entity

    3. %identifier list variables [Optimization]
	The expression .:~(.,(?,Status)) does not yield any pivot as it is
	negated.

	Tagging guards CLEAR upon their last condition being removed would
	allow the EN expression above to traverse the list of CLEAR guards
	rather than all %(?,Action), which would improve performances

	It would be furthermore consistent with the execution principle of
	cellular automata:
		in state
			on event
				do action
	Where
		a cosystem's "current" state is hereby defined as the set
		of its CLEAR guard conditions

	Implementation is illustrated by the pseudo-code below, where
	the %tag and %guard list variables are used to maintain our list
	of CLEAR guards, using

		|^list	adds current entity to list
		|^list~	removes current entity from list
		.%list~:{ expression }
			flushes list while performing expression, in which
			^. references each previously held list element

		Note that pre-frame execution is guaranteed for
			.%tag~:{ ^.:~%(.,(?,Status))|^guard }
		which is not the case for
			do ( %tag~|{ %|:~%(.,(?,Status))|^guard } )

	:Cosystem
		on init
			...
			// %tag init: scout state guard candidates
			...
		else
	+	// %tag update: flush tag list & enrol state guards, pre-frame
		.%tag~:{ ^.:~%(.,(?,Status))|^guard }

		// execute actions according to state guards and trigger EEVA
		per ( %( %guard, ( ?:~%(~%<.>,?), Guard )), ( ?, Trigger ))

			// execute action - assuming %?:[ occurrence, ON|OFF ]
			do : %(%?:(?,.)) : %(%?:(.,?))

			// remove action's corresponding condition from the status of
			// all guards for which it is a condition & tag guard
			in ?: ( %?, %% ) // action's corresponding condition
				do ~( %?, ( %(%?,?):%(?,(.,Guard))|^tag, Status ))

			// add complementary condition to the status of all guards
			// for which it is a condition & remove guard from state
			in ?: (( %(%?:(?,.)), ~%(%?:(.,?)) ), %% )
				do ( %?, ( %(%?,?):%(?,(.,Guard))|^guard~, Status ))

		// update all guard status from EENO - for next frame
		per : ? : . < .
			// remove condition from the status of all guards for which
			// it is a condition & tag guard
			in ?: (( %<?>, %<!:(.,?)> ), %< )
				do ~( %?, ( %(%?,?):%(?,(.,Guard))|^tag, Status ))

			// add complementary condition to the status of all guards
			// for which it is a condition & remove guard from state
			in ?: (( %<?>, ~%<!:(.,?)> ), %< )
				do ( %?, ( %(%?,?):%(?,(.,Guard))|^guard~, Status ))

    Note
   	Special cases: action has no trigger, resp. trigger has no guard

	What we call "actions" here are simply the setting and unsetting of certain
	System's condition, aka. occurrence.

	. An action which has no trigger means that the corresponding System's
	  condition is set resp. unset only once at init - which btw would be a
	  trigger - after which it no longer changes.

	. A trigger always has a guard - as unnamed base entity - which may not have
	  any conditions, in which case, whether or not we have a ( guard, Status )
	  relationship instance, that guard will always verify ~%(.,(?,Status)) - it
	  is therefore only a matter of proper %tag list initialization.

	Needless to say list variables are risky business, not to be generalized, as
	it is left to the programmer to ensure that the entities they are referencing
	are actually accessible - esp. not released.

	But our Use Case here justifies their usage, as in reality things just change,
	they do, in one go, and this is as close as we can get to reflecting reality
	with the tools at our disposal.


