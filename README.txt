Name
	Consensus

Overview
	The purpose of Consensus is to allow to build Systems of information which are
	sensitive, that is: which subscribe to and rely upon each other's changes
	to inform their own states, as opposed to trying to change the other Systems'
	states to have them align with and preserve their own.

	We believe the sensitive approach to be, not only the only sensible approach
	to represent and understand the way Systems work in reality, but also the only
	approach that gives us the chance to harmonize ourselves with our environment.

	The challenge posed by this objective breaks down into two parts
		1. The Procedural part - aka. Consensus Execution Model
		2. The Descriptive part - aka. Consensus Data Model

	1. Consensus Execution Model
	Consensus programs execute according to the following Design Principles:

	   . A System is entirely defined by the series of its States.
	   . The State of a System is the Vector of States of its Subsystems.
	   . Events and Actions are State changes, i.e.
		event = S:a->b	where S is a System and a, b two of its States
		action = S:a->b	where S is a System and a, b two of its States
	   . The General State Change Propagation Equation is
		[ S: a->b ]=>[ S': a'->b' ]
		where S and S' are two Cosystems, and a, b, a', b' their States.
	     Note: within the same system (S = S' above), the GSCPE translates as
		The Action exerted on a System is the second order derivative of
		the series of its States - cf. Second Law of Motion (Newton)
	   . A program - aka. System's Narrative - takes the following form:
		in current State: Condition1
			on change [ from previous ] to current State: Event1
				do change [ from current ] to next State: Action1
		in current State: Condition2
			on change [ from previous ] to current State: Event2
				do change [ from current ] to next State: Action2
		etc.
	  Where
	   . a State represents the contents of [a portion of] the memory, whose
	     differential is tracked at each program step (frame)
	   . external events are mapped into internal events - System's WorldView

	The example source code under the ./Examples/System directory demonstrates how
       	such systems can be architected using linear programming languages such as C.

	The source code under ./Pilot and documentation under ./Pilot/doc show how
	the Web-enabled Consensus Knowledge Management Platform has been architected
	following these same principles.

	2. Consensus Data Model
	The Consensus data model aims at being simple and universal, and relies on the
	Consensus entity as fundamental building block (record type) of the Consensus
	database (CNDB).

	Internally, to date, Consensus relies on the notions of lists, items and entities.

	2.1. Consensus lists and items
	Consensus lists and items address directly the Consensus first Design Principle:
		"A System is entirely defined by the series of its states"
	where
		. the term 'list' in the implementation stands for the term 'series'
		  in the requirement
		. a list item is a pair whose first element is a pointer to the item's
		  data (e.g. 'state' in the requirement), and second element a pointer to
		  the next item in the list
		. From a programming perspective, a list is none other than its first item

	2.2. Consensus entities
	Consensus entities extend the notion of Consensus items to allow multi-dimensional
	connections and, originally, triple-based aggregation, by incorporating the full
	topological information related to each entity in the entity definition itself.

	One of the driving objectives of the on-going Consensus development efforts is to
	harmonize Consensus internal and external representations - so that, ultimately,
	the language can implement itself.

	One if not the most important lesson learnt from the Consensus Pilot development is
	that not 'triples', but 'couples', must constitute the basis for our data model.

	The source code under ./Release/Version-1.0 demonstrates a first industrial-quality
	release of the Consensus programming language, where the notion of instance covers
	both the notions of entity (name) and relationship (couple).

	The language itself has been named B%, say B-mod, for Behavioral Modeling.

