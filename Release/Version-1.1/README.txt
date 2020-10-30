Name
	Consensus Programming Language (B%) version 1.1 - IN PROGRESS

Description
	The objective of this version is to extend the language to support a full
	transducer example implementation - starting from the example provided, in
	C code, under the ./Examples/Yak directory of this site.

	A key target outcome is to allow the definition and usage of "functions",
	or methods, as part of the language.

	A transducer is an excellent case study in that respect, as it requires
	several entities of the same type (rule and schema narrative instances)
	running in parallel and keeping all possible input interpretations.

	The first example will simply allow to apply a scheme:{ rule:{ schema } }
	description onto an input stream, and output the result as the input events
	encapsulated within the structure - aka. "segmentation".

	A later example will show how to convert such result from one structure
	into another - be it internal data - thereby effectively performing
	transduction.

Contents
	This release directory holds the latest source code for this version (under
	development).

	The ./design sub-directory holds both external and internal specifications,
	as well as documentation for this release.
	The ./design/yak.story file is a prototype, in B% pseudo-code, of the first
	target example implementation.
	The ./design/schema-fork-on-completion .jpg and .txt files describe the
	entity behavior & relationship model (EBRM) underlying that example.

	More contents will be provided as work progresses. Watch out for updates
	below in order to track the release status.

Updates
	2020/10/30
		added ./design/specs/db-traverse-implementation.txt
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

