Name
	Consensus Programming Language (B%) version 1.0

Usage
	./B% program.story

Contents

	A. Consensus Fundamentals
	B. Input & Output (version 1.0)
	C. Variable Assignment
	D. Consensus Queries
	E. Example: Turing Machine

A. Consensus Fundamentals

	Consensus implements the following, nonlinear programming language:

		in condition
			on event
				do action

	Where

	1. 'condition', 'event' and 'action' are expressions referring to instances,
	   where an instance can be either an entity (name) or a relationship (couple).
	   The 'expression' format can thus be represented, recursively, as comprised
	   of the following alternatives:

		identifier
		( expression )
		( expression, expression )
		~ expression
		~ ( expression )
		~ ( expression,  expression )
		* expression
		* ( expression )
		* ( expression,  expression )
		%( expression )
		%( expression,  expression )
		expression : expression

	   where 'identifier' can be either one of the following:

		A name - any combination of letters, numbers, and underscore '_' symbols
		The dot '.' symbol - representing "any"
		The star '*' symbol - see below variable assignment operation
		The query '?' symbol - if expression is in the form '%( sub-expression )'

	   Note that only one query symbol is allowed per sub-expression, unless it is
	   embedded in a sub-sub-expression.

	2. The narrative statement 'in condition' probes the Consensus Database (CNDB) and
	   passes (that is: allows the execution of the underlying statements) if and only
	   if there exists at least one instance verifying the 'condition' expression.

	3. The narrative statement 'on event' probes the Consensus program’s frame log
	   (CNLog) and passes if and only if there exists at least one instance verifying
	   the 'event' expression and which, based on the following convention, was either
	   created or released during the last program frame:

		if the 'event' expression is in the form '~( expression )' then 'on event' 
		will probe the CNLog for at least one instance verifying 'expression' and
		which was released (de-instantiated) during the last frame.

		if the 'event' expression is not in the form '~( expression )' then
		'on event' will probe the CNLog for at least one instance verifying the
		'event' expression and which was created (instantiated) during the last
		frame.

	4. The narrative statement 'do action' causes either new instances to be added to,
	   or existing instances to be removed from, the CNDB, based on the following
	   convention:

		if the 'action' expression is in the form '~( expression )' then 'do action'
		will search the CNDB for all instances verifying 'expression' and, for each
		instance found: if the instance is an entity, remove the entity from the
		CNDB; otherwise if the instance is a couple, remove the couple’s
		relationship instance from the CNDB.

		if the 'action' expression is not in the form '~( expression )' then
		'do action' will attempt to instantiate all entities and relationships
		needed to substantiate the 'action' expression. It will fail only if one
		of the 'action' expression terms is a query yielding no result.

	   Consensus 'do' operations only take effect after the end of the current frame
	   and before the start of the next one.

	   Note that the removal of an instance entails the removal of all its descendants.

B. Input & Output (version 1.0)

	In addition to the above-described action expressions, and specific to this version,
	Consensus allows the user to specify the following actions:

		do expression : <

	This action attempts to read a name (see above) from the program’s standard input
	channel, and, if it succeeds, instantiates '(( *, expression), name )'. Otherwise,
	if it cannot read a name, upon reading EOF the Consensus input operation releases
	the '( *, expression )' relationship instances and all its descendants.

		do > format : expression

	This action outputs 'expression' according to 'format' to the program’s standard
	output channel, where 'format' can be any sequence of characters enclosed in double
	quotes (") and where the specific subsequence '%_' refers to the passed expression
	(only one expression per output statement is supported in this version).

C. Variable assignment

	The action 'do (( *, variable ), value )' marks the current '(( *,variable), . )'
	relationship instance, if any, to be released, and the specified new assignment
	to be created.

	In case the 'value' expression represents multiple instances, Consensus will ensure
	that only one of these instances is assigned to 'variable', although the user is
	not given the possibility to predict which one.
	In case the 'variable' expression represents multiple instances, then 'value' will
	be assigned to each of these instances.

	These actions, like all Consensus 'do' operations, only take effect after the end
	of the current frame and before the start of the next one. This allows both the
	'on (( *, variable), value)' and the 'on ~(( *, variable), previous_value )' event
	operations to perform as expected - if 'value' is the same as 'previous_value'
	(reassignment), only the former event operation will pass.

D. Consensus queries

	Consensus query operations are represented by the following expression terms:

		* expression
		%( expression )
		expression : expression
		~ expression

	Where

	1. The term '*variable' is equivalent to the term '%(( *, variable ), ? )'
	   and dereferences the 'variable' expression.

	   Example: the term '*.' represents all assigned instances (values).
	
	2. The term '%( expression )' represents all instances which verify
	   'expression' from the expression's mark '?' standpoint. If 'expression'
	   has no mark, then '%( expression )' represents all instances verifying
	   'expression'. 

	   Example: the term '%( ? )' represents all CNDB instances.

	   Special case: the term '%( *? )' represents all instances which are used
	   as references (variables).

	   Note here the difference between, for example, the following expressions:

		do ( name1, ( name2, . ))
		do ( name1, %( name2, . ))

	   The former will couple 'name2' with all existing instances in the CNDB,
	   prior to coupling each resulting instance with 'name1'; whereas
	   The latter will couple only existing '( name2, . )' relationship instances
	   with 'name1'.

	3. The term 'expression : expression' represents the instances which verify
	   both 'expression' terms.

	4. The term '~expression' represents the instances which do not verify
	   'expression'.

E. Example: Turing Machine

	The directory ./Examples/TuringMachine of this site features a Turing Machine[1]
	implementation based on the above-described Consensus programming language
	(the language itself has been named B%, say B-mod, for Behavioral Modeling)
	thereby demonstrating the Turing completeness of the language.

	More examples can be found e.g. in the ./Release/Version-1.0/Test directory of this
	site, in particular the CN.suite program (aka. story in Consensus terms), which
	provides a good starting point for learning and experimenting with the language.

	References:
	1. Turing machines, an Introduction - Pascal Michel, Maitre de Conference,
	   Univ. Cergy-Pontoise, France - http://www.logique.jussieu.fr/~michel/tmi.html

