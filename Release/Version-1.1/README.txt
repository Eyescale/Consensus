Name
	Consensus Programming Language (B%) version 1.1

Usage
	./B% program.story
	./B% -p program.story
	./B% -f CNDB.init program.story

Description
	The objective of this version was to extend the language to support a full
	transducer example implementation - starting from the example provided, in
	C code, under the /Base/Examples/Yak directory of this site.

	A target outcome was to allow the definition and usage of "functions" - or
	methods - as part of the language. A transducer was an excellent case study
	in that respect, as it requires multiple entities of the same type, namely
	rule and schema instances, to run in parallel - keeping / exploring all
	possible input interpretations.

	Rather than introducing functions, our investigation led us to refine the
	notion of "narrative instance", whereby a narrative, when it is in execution,
	is associated with an ( entity, CNDB ) relationship instance - a story now
	consisting of a collection of narratives.

	An additional benefit was that it solved the issue of how to formalize loops
	while remaining consistent with the driving principle of our research - which
	is to rely on the biological capabilities of neurons, as far as we know them.

	While "loops in time" do not make sense, one entity/neuron may - and does -
	have multiple instantaneous connections, the parallel execution of which we
	want to specify. The newly introduced notion of "narrative instance" enables
	us to do so without further extending the language.

	Note that we may still introduce "loops" in later versions to process query
	results, using the keyword 'per' to avoid confusion with loops in time,
	e.g.
		do per ?: expression
			action
	where
		action is considered atomic, all instances executing at the same time, and
		possibly consisting of a whole per-result-specific sub-narrative.

Contents
	The Consensus B% Version-1.1 Programming Guide can be found under the newly
	created Release/Documentation directory of this site.

	The ./design sub-directory holds both external and internal specifications.

	The ./design/yak.story file is a prototype, in B% pseudo-code, of the first
        targeted implementation - the fully functional, final implementation of which
	can be found under the Release/Examples/1_Schematize directory of this site.

	The ./design/schema-fork-on-completion .jpg and .txt files describe the
	entity behavior & relationship model (EBRM) underlying that design.

	The released examples allow to apply a scheme:{ rule:{ schema } } description
	onto an input stream, and output the result as the input events encapsulated
	within the structure - process generally known as segmentation, but which we
	refer to as schematization.

	The next release will focus on ways to convert the schematization results
	from one structure to another - be it internal data - thereby effectively
	performing transduction.

