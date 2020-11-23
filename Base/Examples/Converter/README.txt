
Name
	./Examples/Converter

Synopsis
	The sample code in the ./Examples/Converter directory provides
	an example of how to implement a converter using the Consensus
	Yak transducer.

Usage
	./convert [-n]

Description
	The program converts Consensus 'in condition' statements provided
	by the user into their equivalent C counterpart, according to the
	grammar specified in the ./Examples/Converter/scheme.y resource file
	and example usage given in the ./Examples/System sample code.

	User inputs not starting with 'in ' or 'in(' or 'in:' or 'in~' are
	reproduced as they are by the program - the idea being that the
	converter will translate Cn statements from within C files and leave
	the C part of the code unmodified.

	The option -n invokes a different, non-tokenized implementation of
	the converter to provide exactly the same results.

Examples
	yak1$ in : state
	if (CNIni(state,1)) {
	yak2$ in :~ state
	if (CNIni(state,0)) {
	yak3$ in : state : 123
	if (CNIni(state,123)) {
	yak4$ in ~( : state : 123 )
	if (!(CNIni(state,123))) {
	yak5$ in { : state : 123, : variable : 456 }
	if (CNIni(state,123) || CNIni(variable,456)) {
	yak6$ in ( :~ stateA, { :stateB, :stateC:123 } )
	if (CNIni(stateA,0) && (CNIni(stateB,1) || CNIni(stateC,123))) {
	yak7$ hello, world\n
	hello, world\n
