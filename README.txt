Name
	Consensus

Overview
	The purpose of Consensus is to allow to build Systems of information which are
	sensitive, that is: which subscribe to and rely upon each other's changes
	to inform their own states, as opposed to trying to change the other Systems'
	states to have them align with and preserve their own.

	The challenge posed by this objective breaks down into two parts
		1. The Procedural part - aka. Consensus Execution Model
		2. The Descriptive part - aka. Consensus Data Model

	1. Consensus Execution Model
	The fundamental principles pertaining to the Consensus Execution Model are
	exposed very simply in the example located in the ./Examples/System directory.
	The application of these principles for building a Web-enabled Consensus
	Knowledge Management Platform is depicted in ./Pilot/doc/ReleaseNotes.pdf

	2. Consensus Data Model
	Whereas the Consensus Execution Model is relatively straightforward - it
	generalizes the notion of State Machine based on the Consensus general state
	change propagation equation - its Data Model is more complicated.
	This is due to the fact that the internal and external data representations
	have not yet been harmonized, their harmonization being the purpose of the
	on-going Consensus research and development efforts.
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
	connections and triple-based aggregation. They incorporate the full topological
	information related to each entity in the entity definition itself.

	The Consensus data model aims at being simple and universal, and relies on the
	Consensus entity as fundamental building block (record type) of the Consensus
	database (CNDB). One if not the most important lesson learnt from the Consensus
	Pilot development is that not 'triples', but 'pairs', must constitute the basis
	for our data model. This will allow us, in the next version, to get rid of brackets
	and arrows, and provide the user with a syntax even closer to natural language to
	describe, navigate and manage his data.

	-- WIP --
