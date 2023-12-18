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
	Also
		%<.> represents External Event Variable Assignment (EEVA) hard-coded
		     aka. %<< : ^((?,.),.) : ^((.,?),.) < ^(.,?) >>

	: Cosystem
		on init
			...
		else
			// enable actions based on guard->status and trigger->event EEVA
			%( .:~%(.,(?,Status)), ( .:~%( ?:%(.:~%<.>,?), ( ?, Trigger )), Guard ))
			// ^---- guard           ^---- trigger           ^---- action

			// update all guard status from EENO - for next frame
			per : ? : . < .
				// remove condition from all guard.status
				in ?: (( %<?>, %<!:(.,?)> ), %< )
					do ~( %?, ( ., Status ))

				// add complementary condition to the status of the guards
				// for which it is a condition
				in ?: (( %<?>, (%<!:(.,ON)>?OFF:ON) ), %< )
					//--- guard ----v       v---- trigger
					do ( %?, ( %(%?,?):%(?,(.,Guard)), Status ))

	.action: %( ?, Action )	// alt. %(.,( ?, Trigger ))
		// execute action - assuming action:[ occurrence, ON|OFF ]
		do : %(action:(?,.)): %(action:(.,?))

		// remove action's corresponding condition from all guard.status
		in ?: ( action, %% ) // %? is condition
			do ~( %?, ( ., Status ))

		// add complementary condition to the status of the guards
		// for which it is a condition
		in ?: (( %(action:(?,.)), (action:(.,ON)?OFF:ON) ), %% )
			//--- guard ----v       v---- trigger
			do ( %?, ( %(%?,?):%(?,(.,Guard)), Status ))

Performance
	The condition .:~(.,(?,Status)) above, being negated, does not yield
	any pivot.

	Flagging guards CLEAR upon their last condition being removed would
	allow the EN expression above to traverse all %(?,CLEAR) guards rather
	than all %(.,(?,Trigger)). Furthermore it would be consistent with
	the cellular automaton execution principle

		in state*
			on event
				do action

 	*a cosystem's "current" state being here defined as the set of its
	CLEAR'd guard conditions.

New Feature Requirements
    1. Unnamed base entities
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

	Release
		Either directly by user or automatically when left dangling - in
		which case no release event is issued

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

    2. Expression terms conditional EENO
	We want to use conditional EENO %<< event < src >> inside expressions,
	so that
		.:%<< event < src >> passes <=> the EENO is verified
		.:~%<< event < src >> passes <=> no such EENO

	The symbol '^' can be used inside the EENO to represent the entity
	corresponding to the current expression term

	Example:
		to verify that the current expression term matches an external
		variable value assignment (EEVA) event, in the form
			 : a : b < c
		such that 
			^((?,.),.): a	// current.sub[0].sub[0]
			^((.,?),.): b	// current.sub[0].sub[1]
			^(.,?): c	// current.sub[1]
		we would write
			%<< : ^((?,.),.) : ^((.,?),.) < ^(.,?) >>
	Note:
		We use the syntax ^((?,.),.) instead of e.g. %(%.:((?,.),.) as
		%. representing the entities matching the expression's current
		term may hold more than one result, and therefore %(%.:((?,.),.))
		and %(%.:((.,?),.)) might not relate to the same entity
	Shortcut:
		%<.> stands for the above-described conditional EENO
			%<< : ^((?,.),.) : ^((.,?),.) < ^(.,?) >>

