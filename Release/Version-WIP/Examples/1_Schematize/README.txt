Name
	Consensus/Release/Version-2.0/Examples/1_Schematize

	UPDATE Version-2.0:
	. adapted to new input logics, whereby EOF causes : input : ~.
	. adapted to new active logics, whereby
		on ~.		// always fails
		on ~.:.		// means idle event
	. created schematize.new using Version-2.0 features
	. created yak.new with extracted cell's intake process

Usage
	../../B% -f Schemes/yak.ini yak.story
	../../B% -f Schemes/file schematize
	../../B% hello_world.story

Description
	This directory holds the examples targeted by the Version-1.1 release
	development

	The yak.story and schematize examples allow the user to specify a full
	formal grammar consisting of a collection of named Rules, each Rule made
	of Schemas which in turn — using the ( %, identifier ) relationship
	instance — may reference Rules.

	The input read from stdin is then output to stdout, with the pattern-matching
	sequences encapsulated within the structure - process generally known as
	segmentation, but which we refer to as schematization.

	The schematize story differs from the yak.story example in that
	1. the former uses the construct (( identifier, % ), pattern ) instead of
	   the construct (( Rule, identifier ), ( Schema, pattern )) which is used
	   in the latter.
	2. the schematize story uses a do ( | pipe command instead of two separate
	   commands to launch a rule and associated schema threads.

	The hello_world.story example only allows the use of Schemas, without any
	cross-referencing possibility. This example is presented in more details
	in the Consensus B% Programming Guide located in the Release/Documentation
	directory of this site, and is intended to facilitate the reader’s
	understanding of the other two, more advanced programs.

See Also
	The Consensus/Release/Version-1.1/Examples/1_Schematize/README.txt document
	and Consensus/Release/Version-1.1/design directory of this site provide more
	insight into the architecture of these examples.

	. The design/yak.story file is a prototype, in B% pseudo-code, of the
	  first targeted implementation - the final implementation of which is
	  located in the present directory.

	. The design/schema-fork-on-completion .jpg and .txt files describe the
	  entity behavior & relationship model (EBRM) underlying that design.

