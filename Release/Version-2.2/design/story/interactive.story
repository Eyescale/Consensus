Usage
	./B% -i

Objective
	Our goal is to allow the user to, interactively

	. specify systems simply in terms of cosystems and occurrences,
	  where cosystem occurrences = events, conditions and actions

	. navigate their system status and dependencies both statically
	  and dynamically, that is: when the system is in action

	further bridging the gap between natural language and applications
	software development.

Data Model
	We want our System to rely on the following Entity Relationship Model

					O
				       -:-
				       / \
	 .---> condition .
	 |	  ...      } guard ----.
	 .---> condition .	        |
	 |			        |
	 |			        |
	 |  .---> event .	        v
	  --|	    ...   } trigger ----+---> action ----.
	    .---> event .			          |
	    ^ 					          |
	    | 					          |
	     ------------- [ B% Narrative ] --------------


				     Machine
	where
		action: (( occurrence, /(ON|OFF)/ ), cosystem )

	. Occurrences are statements representing cosystem conditions
	. Each occurrence doubles as (occurrence,{ON,OFF}) to allow the user
	  to specify active as well as negated condition

	A System therefore is entirely defined by the set of relationship
	instances

		( .guard:!!, ( .trigger:(!!,!!), .action ) )
			 ^		 ^  ^------- bar:=~on
			 |		  ---------- on [event]
			  -------------------------- in [condition]

		!! references a Consensus B% Unnamed Based Entity (UBE)
	where
		action: (( occurrence, /(ON|OFF)/ ), cosystem )
		event: %( ?:~!!, !!:{ %(trigger:(?,.)), %(trigger:(.,?)) } )
		condition: %( ?, !!:guard)
		events and conditions themselves in the form 
			(( occurrence, /(ON|OFF)/ ), cosystem )

Description
	Requirement Specifications can be reduced to short statements, which
	we call occurrences, describing the System's conditions, events,
	and actions, according to the syntax

		in occurrence(s) // condition
			on occurrence(s) // event
				do occurrence(s) // action

	The System's Requirement Specification process involves identifying,
	for each action occurrence

	. the referent, aka. cosystem responsible for executing the action
	. the action's set of triggers, each trigger composed of
		its triggering set of occurrences ON|OFF (events)
		its enabling set of guards, each guard composed of
			its defining set of occurrences ON|OFF (conditions)

System Execution
	Once launched, we want our System to execute the following, which
	we call System frame, over and over again:

	        for each action candidate
	            for each candidate<-trigger
	                if all trigger events are verified
	                    for each guard in trigger<-guard
	                        if all guard conditions are verified
	                            execute action // only once per frame
	                        end if
	                    end for
	                end if
	            end for
	        end for

	In Consensus terms, as in reality, all cosystems run in parallel, and
	we want all actions to likewise execute in parallel, all in one frame.
	Furthermore, we do not want each cosystem to keep track of all other
	cosystems conditions - although we could - but rather: each cosystem
	will maintain its own action<-trigger<-guard.status, according to
	the following pseudo-code:

	: Cosystem
		per action: (( occurrence, ON|OFF ), %% ) SUCH THAT
		    there is one guard among action<-trigger<-guard
		    verifying BOTH
		    . guard.status is clear, AND
		    . there is one trigger among guard->trigger for which
		      ALL trigger<-event are occurring

				execute action // do : occurrence : ON|OFF

				remove action's corresponding condition from all
				relevant cosystem guard.status // for next frame

				add complementary condition (( occurrence, ~ON|OFF ), %% )
				to all relevant cosystem guard.status // for next frame

		per : ? : . < .	// update next frame's conditions
			remove EENO's corresponding condition (( %<?>, %<!:(.,?)> ), %< )
			from all relevant cosystem guard.status // for next frame

			add complementary condition (( %<?>, ~%<!:(.,?)> ), %< )
			to all relevant cosystem guard.status // for next frame

	Which, in B%, translates as follow:

	: Cosystem
		on init
			// initialize local ( conditions, ( Status, guard ))
			per ( ?, ( ., ((.,ON|OFF),%%) ) )
				do ( %(?,%?), ( Status, %? ))
			...
		else
+		per ( .:~%(.,(Status,?)), ( .:~%(~EEVA,?), (?:(.,ON|OFF),%%) ))
			// execute action
			do : %(%?:(?,.)): %(%?:(.,?))

			// remove action-corresponding condition from guard.status
			do ~( (%?,%%), %( Status, . ))

			// add action-complementary condition to relevant guard.status
			in ?: (( %(%?:(?,.)), ~%(%?:(.,?)) ), %% )
				do ( %?, %( Status, %(%?,?) ))

		per : ? : . < .
			// remove eeno-corresponding condition from all guard.status
			in ?: (( %<?>, %<!:(.,?)> ), %< )
				do ~( %?, %( ., Status ))

			// add eeno-complementary condition to relevant guard.status
			in ?: (( %<?>, ~%<!:(.,?)> ), %< )
				do ( %?, %( Status, %(%?,?) ))
	where
		. Cosystem-relevant System data have been made accessible
		  to cosystem at System's launch time (see below)
		. EEVA stands for External Event Variable Assignment and is
		  fully defined in the Feature List section below
		. The expression ~(.,(Status,?)) does not yield any pivot as
		  it is negated.

	Tagging guards CLEAR upon their last condition being removed will
	allow us to use the CLEAR guards list as a pivot, AND is further
	consistent with the cellular automata execution principle
		in state
			on event
				do action
		whereby a cosystem's "current" state is defined as its set
		of CLEAR guard conditions

	This will be implemented through the new %identifier list variables
	B% capability, featuring
		|^list		adds current entity to list
		|^list~		removes current entity from list
		.%list~:{_}	flushes list while performing expression, in
				which ^. references the current list element
	as per the following, final target Implementation:

	: Cosystem
		en %(*?:ON) // enable cosystem occurrence's sub-narratives
		on init
			// initialize ( &condition, ( Status, &guard )), and
			// enrol condition-free guards into %guard list
			per ( ?, ( ., ((.,ON|OFF),%%) ))
				do ( (.,%?) ? // guard has condition(s)
					( %(?,%?), ( Status, %? )) :
					%( %?|^guard ) )
		else
	+	// flush %tag list & enrol condition-free guards, pre-frame
		.%tag~ { ^.:~%(.,(Status,?))|^guard }
	
		// see the Feature List section below for EEVA definition
		per ( %guard, ( ~%(~EEVA,?), (?,%%) ))
	
			// execute action
			do : %(%?:(?,.)) : %(%?:(.,?))

			// remove action-corresponding condition from relevant guard
			// Status, and tag these guards
			do ~( (%?,%%), %( Status, .|^tag ))
	
			// add action-complementary condition to relevant guard
			// Status, and decommission these guards
			in ?: (( %(%?:(?,.)), ~%(%?:(.,?)) ), %% )
				do ( %?, %( Status, %(%?,?)|^guard~ ))

		// update all guard status from EENO - for next frame
		per : ? : . < .
			// remove eeno-corresponding condition from relevant guard
			// Status, and tag these guards
			in ?: (( %<?>, %<!:(.,?)> ), %< )
				do ~( %?, %( Status, .|^tag ))
	
			// add eeno-complementary condition to relevant guard
			// Status, and decommission these guards
			in ?: (( %<?>, ~%<!:(.,?)> ), %< )
				do ( %?, %( Status, %(%?,?)|^guard~ ))
	
	: cosystemA < Cosystem	// note the sub-classing
		...
	:"occurrence" // invoked when occurrence is ON
		...
	:"occurrence" // invoked when occurrence is ON
		...
	:"occurrence" // invoked when occurrence is ON
		...
	
	: cosystemB < Cosystem	// note the sub-classing
		...
	...

	Notes
	1. each cosystem is effectively a class of its own, subclass of Cosystem
	2. guards matching %( Status, ? ) refer to local actions - no check needed
	3. guards with no condition have no Status - but are enrolled permanently
	4. if a trigger has no event ~%(~EEVA,?) always passes - as intended

	A centralized execution model implementation is also feasible, whereby
		. system notifies cosystems of their expected actions
		. cosystems notify system of their actually performed actions
		  (whereupon system updates status)
	but that is not our focus here.

System launch
	Each cosystem needs a local system image integrating everything leading
	to the cosystem's actions, that is:
		( guard, ( trigger, ((occurrence,ON|OFF),cosystem:%(*?:%%)) ) )
	as well as
		( ., trigger )	// events - for selected triggers
		( ., guard )	// conditions - for selected guards

	Additionally all connections with event cosystems must be established,
	and we want proxies to replace cosystem identifiers in system images -
	which requires all proxies to be created beforehand.

	This leads us to the following System launch expression:

	do : %cosystem : !! Cosystem(
		// foreach action instantiate same (with proxy vs. cosystem), for which
		?:(( ., ON|OFF ), ^^ ) { ( %?::(?,.), *^^ ) |
			// foreach (trigger, action) instantiate same, for which
			?:( ., %? ) { ( %?::(?,.), %| ) | {
				// for each (event,trigger) instantiate same (with proxy vs. cosystem)
				?:( ?, %?::(?,.) ) (( %?::(?,.), *^%?::(.,?) ), %|::(?,.)),
				// foreach guard:%(?,(trigger,action)) instantiate same, for which
				?:( ?, %? ) { ( %?, %| ) |
					// for each (condition,guard) instantiate same (with proxy vs. cosystem)
					?:( ?, %? ) (( %?::(?,.), *^%?::(.,?) ), %|::(?,.)) }}}})

	That is, without the comments:

	do : %cosystem : !! Cosystem(
		?:((.,ON|OFF),^^) { ( %?::(?,.), *^^ ) |
			?:(.,%?) { ( %?::(?,.), %| ) | {
				?:( ?, %(%?::(?,.)) ) (( %?::(?,.), *^%?::(.,?) ), %|::(?,.)),
				?:(?,%?) { ( %?, %| ) |
					?:(?,%?) (( %?::(?,.), *^%?::(.,?) ), %|::(?,.)) }}}})
	where
		do : %cosystem : !! Cosystem(_) is internally bufferized
		^^ represents the current assignee's value in the assignment buffer
		*^^ represents the current assigner's value in the assignment buffer
		*^%?:sub represents %?:sub's value in the assignment buffer

	Note
	. *(^^) and *%?:sub would represent resp. ^^'s and *%?:sub's values in CNDB,
	  whereas *^^ and *^%?:sub represent these values in the assignment buffer

System init
	See implementation in
		Examples/6_System/system.story
		Examples/6_System/include/cosystem.bm

	Note: since :event:OFF is not the same as ~.::event:ON, we actually
	use a double-trigger:(bar,on), and the following modification

	per %( %guard, ( .:(~%(%<.>,?),~%(~%<.>,?)), ? ))
			 ^  ^	       ^----- bar: none of these EVA passes
			 |   ---------------- on: none of these EVA does not pass
			  ------------------- trigger

Feature List
    	1. %identifier list variables
    	2. <<_>> EENO Condition (EENOC) and EEVA definition
	3. !! Unnamed Base Entities (UBE)
	4. Shared Entities and Shared Arena implementation

    1. %identifier list variables

	|^list		adds current entity to list
	|^list~		removes current entity from list
	.%list~:{_}	flushes list while performing expression, in
			which ^. references the current list element

	Note that pre-frame execution is guaranteed for
		.%tag~ { ^.:~%(.,(?,Status))|^guard }

    2. <<_>> EENO Condition (EENOC) and EEVA definition
	In the code above, EEVA represents the following EENO Condition

		<< : ^((?,.),.) : ^((.,?),.) < ^(.,?) >>

	Where
		1. << event < src >> passes iff event < src is verified

		2. The symbol '^' represents the entity corresponding to
		the current expression term, hence

			<< : ^((?,.),.) : ^((.,?),.) < ^(.,?) >>

		expresses that, in order to pass, the current expression
		term must match an external event in the form
			: a : b < c
		such that 
			a: ^((?,.),.)	// current.sub[0].sub[0]
			b: ^((.,?),.)	// current.sub[0].sub[1]
			c: ^(.,?)	// current.sub[1]

	Note that we use the syntax ^((?,.),.) instead of %(%.:((?,.),.)
	as %. representing expression's current term could represent more
	than one entity, and therefore %(%.:((?,.),.)) and %(%.:((.,?),.))
	might not relate to the same entity

    3. !! Unnamed Base Entities (UBE)
	To spare us the hassle of managing our own pool of free Trigger and
	Guard entities, we want the capability to allocate each Trigger and
	Guard instance as unnamed base entity (UBE).

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

    4. Shared Entities and Shared Arena implementation
    4.1. Shared Entities
	We need to be able to access entities by address in order to
	1. optimize string memory usage AND comparison mechanism across system
	2. allow multiple references to the same unnamed base entity - here a
	   trigger or a guard - in cosystems' system images

	Implementation: instead of
		CNDB.entry:[ identifier, e:(NULL,identifier) ]
	we will have
		CNDB.entry:[ identifier, e:(NULL,entry) ]
	so that: if e->sub[1]->value!=e then
		e is a reference to an external entity => in shared arena,
		as we cannot have several entries with the same identifier
		in the same CNDB

    4.2. Shared Arena
	We have
		CNStory:[ Registry *narratives, Registry *arena ]
	where
		arena: { [ "",  (Registry *) UBE-arena ],
			 [ "$", (listItem *) string-arena ] }
		UBE-arena.entry: [ NULL, {[ CNDB, entity ]} ]
			where entity: CNDB-specific ( NULL, item )
		string-arena.entry: [ identifier, {[ CNDB, entity ]} ]
			where entity: CNDB-specific ( NULL, entry )

	Note: as entry->name is used to differentiate between UBE and
	string arenas, there is no way around using NULL as item->name
	in UBE-arena

	string-arena is sorted by identifier, whereas UBE-arena is
	sorted by address - of the entry

	UBE-arena items are created either:
	1. using !! => NEW UBE, associated with BMContextDB(ctx)
	2. exporting existing UBE from one CNDB to another, via carry
	   <=> making another CNDB-specific ( NULL, item ) version
	3. importing existing UBE from one CNDB to another, via EENO
	   <=> making another CNDB-specific ( NULL, item ) version

	string-arena entries are created either:
	1. using do "_" => NEW string, associated with BMContextDB(ctx)
	2. exporting existing string from one CNDB to another, via carry
	   <=> making another CNDB-specific ( NULL, entry ) version
	3. importing existing string from one CNDB to another, via EENO
	   <=> making another CNDB-specific ( NULL, entry ) version

	Shared entities are released in context, whereupon
	. the BMContextDB(ctx)-specific version is removed from the relevant
	  entry in the relevant arena
	. if no version is left in that entry after that removal, then
	  the whole entry is removed from the arena and freed

System Rules
	. Each DO or CL action occurrence substantiates its corresponding
	  event and condition occurrences, i.e.
		DO "occurrence" => IN/ON "occurrence" substantiated
	. DO action occurrences may be specified as occurrence originators,
	  via
		DO "action occurrence"
		> "generated occurrence"
		...
	. occurrence-generating action occurrences MUST be associated with
	  a B% narrative generating the event, via
		: "action occurrence"
			...
			in ?: "generated occurrence"
				en .ON|OFF( %? )
	  This includes
		DO "action occurrence"
		> &
	. When an action narrative is specified, then the corresponding
	  CL action can not be specified outside that narrative, i.e.
	  the corresponding OFF event occurrence, if used as IN/ON/OFF,
	  must be generated from inside that same narrative, via
		: "action occurrence"
			...
			en .OFF( this )
	. Conversely neither CL action nor generated occurrences can have
	  a B% narrative, whereas DO action occurrences which are neither
	  also specified as CL action occurrences nor generating occurrences
	  may or may not have associated B% narrative - in the negative, they
	  then substantiate System events/conditions
	. TBD: whether multiple actions can generate the same occurrence(s)
	  and whether a CL occurrence can also be generated remains to be
	  decided, which has bearing on whether CL and generated occurrences
	  require a System cosystem instance, and/or whether generated occurrences
	  pertain to the originating cosystem, in which case
	  . a System cosystem instance is only required in case of CL occurrences
	  . one additional level of indirection is required to translate
	    generated occurrences into system occurrences

Rule Summary
		"Generated cannot be generative, & vice-versa"
	where
		CL occurrences are considered generated occurrences
		DO occurrences are considered generative if they are
		   explicitely specified as generating occurrences

System Completion
	The system is complete when every IN/ON/OFF occurrence has
	a corresponding action or generated occurrence, that is:
	Each event or condition occurrence is either
	  . corresponding to an action specified and in use
	  . specified as originating from an action, via
		: "action occurrence" // B% narrative
			...
			in ?: "event or condition occurrence"
				en .ON|OFF( %? )

Notes
	. If the system is not complete, then the user must be informed as to
	  which IN/ON/OFF occurrence does not have an originator
	. Some action or generated occurrences may not be used as IN/ON/OFF
	  occurrences, in which case the user must be warned

