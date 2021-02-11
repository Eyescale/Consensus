Name
	Consensus Programming Language (B%) version 1.1

Usage
	./B% program.story
	./B% -p program.story
	./B% -f CNDB.int program.story

Description
	The objective of this version was to extend the language to support a full
	transducer example implementation - starting from the example provided, in
	C code, under the /Base/Examples/Yak directory of this site.

	A key target outcome was to allow the definition and usage of "functions" -
	or methods - as part of the language. Our investigations led us instead and
	naturally to refine the notion of "narrative instance", whereby a narrative,
	when it is in execution, is associated with an ( entity, CNDB ) relationship
	instance - a story now consisting of a collection of narratives.

	An additional benefit of this approach is that it allows us to solve the
	issue of how to formalize "loops" without contradicting the driving principle
	of our research - which is to rely on the biological capabilities of neurons,
	as far as we know them.

	While "loop in time" do not make sense, one entity/neuron may - and most
	often does - have multiple instantaneous connections, the parallel execution
	of which we want to be able to specify. The notion of "narrative instance"
	enables us to do so without any further extension to the language.

	Note that we may still want to introduce "loops" in later versions - e.g.
	to process query results - but with a different syntax, so as not to confuse
	them with loops in time, e.g.

		do per ?: expression
			action
	where
		action is considered atomic, all of them executing at the same time,
		possibly consisting of a whole per-result-specific sub-narrative.


	A transducer was an excellent case study in that respect, as it requires
	multiple entities of the same type, namely rule and schema instances, to run
	in parallel - keeping / exploring all possible input interpretations.

	Our main release example simply allows to apply a scheme:{ rule:{ schema } }
	description onto an input stream, and output the result as the input events
	encapsulated within the structure - process generally known as segmentation,
	which we refer to as schematization.

	The next release will focus on how to convert the schematization results from
	one structure into another - be it internal data - thereby effectively
	performing transduction.

Contents
	This release directory holds the latest source code for this version (under
	development).

	The ./design sub-directory holds both external and internal specifications,
	as well as documentation for this release.
	The ./design/yak.story file is a prototype, in B% pseudo-code, of the first
        targeted implementation - the fully functional, final implementation of which
	can be found under the Release/Examples/1_Schematize directory of this site.
	The ./design/schema-fork-on-completion .jpg and .txt files describe the
	entity behavior & relationship model (EBRM) underlying that design.

	More contents will be provided as work progresses. Watch out for updates
	below in order to track the release status.

Updates
	2020/12/02
		engineering complete - part I
		. completed /Base/Release/Examples/Yak with test cases
		. updated design/specs/software-architecture.txt
	2020/11/20
		introduced on ~. meaning no event during last frame
		. needed for detecting deadlock and exiting schema threads
		introduced regex identifiers
		. needed to allow in *event:/[ \t]/ or /[A-Za-z0-9_]/
		. added ./Test/CN.rx
		created ../../Base/sequence.c
		. needed to allow do (( rule, id ), ( schema, { ... } ))
		  and eventually do > format: { arg1, arg2, ... }
	2020/11/13
		feature complete: single character identifiers
		. refactored bm_input and bm_output, e.g. input now
		  support relationship instance identifiers [UNTESTED]
		  as well as single character identifiers (in quotes)
		. patched Version-1.0 and TuringMachine example to
		  resp. recognize and use output format "%s"
		. introduced input formats "%_", "%c" and "%%", and
		  output formats "%_", "%s" and "%%"
		. added ./Test/CN.output
	2020/11/11
		added design/specs/test_hval.c validating output for
		single character identifier entities
		. completed bm_output and bm_register support
		. completed cn_out
	2020/11/10
		added ./Test/CN.yak - usage (main.c compiled with DEBUG):
			./B% ./Test/CN.yak > toto
			./B% toto > titi
			diff toto titi
		. diff should show no difference
		. Note: surely there is a way to write all three lines above as one
		  updated story_output and narrative_output
	2020/11/09
		feature complete: narrative instance registry
		. narrative instance definition [UNTESTED]
		. support .local declarations and do .( expression )
	2020/11/06
		feature change: using .arg instead of %arg in narrative instance name
		. updated design/yak.story, operation.c, util.c and expression.c
	2020/11/04
		feature change: made + and - tab logics relative
		feature complete: mark register
		. supporting in/on ?:expression and %? in narrative registry
		. added ./Test/CN.mark
	2020/11/03
		code refactoring
		. implemented changes according to ./design/specs/code-analysis.txt
		. added util.c and include/util.h
	2020/11/02
		code cleanup
		. simplified VerifyData - interface to bm_verify / bm_match
		. optimized bm_match() - using pivot from db_traverse() & db_feel()
		. changed init and exit from operation flags to CNDB entities
	2020/10/31
		added ./design/specs/code-analysis.txt
		added ./design/specs/read-narrative-states.txt
	2020/10/30
		added ./design/specs/db-traverse.txt
		simplification
		. implemented input preprocessor in narrative.c
	2020/10/29
		added ./design/specs/software-architecture.txt
		code cleanup
                . moved db_feel() out of database.c, into traversal.c
                . now use prefix bm_ for language-specific functions
	2020/10/28
		added ./design/task-list.txt
		feature complete: line continuation after \
			. also applies to format string
			. added ./Test/CN.backslash
		feature complete: standalone % entity
			. modified narrative.c expression.c
			. added ./Test/CN.percent
		added code base from ../Version-1.0
			. including bugfix strmatch()
			. supporting narrative statements ending with *
			  added Test/CN.star
	2020/10/27
		added ./design/specs/execution-model.txt
	2020/10/24
		added ./design/feature-list.txt
		added ./design/specs/single-character-entity.txt
	2020/10/23
		added ./design/specs/entity-list.txt

