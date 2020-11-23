Name
	./Yak

Synopsis
	Yak is a transducer - a la Yacc, Bison, PLY ...
	A yak scheme file allows to specify the target input grammar in
	the form of rules and schemas, and corresponding output tokens.
	Yak allows regular expressions (regex) to be used as part of the
	scheme file format.

File Format
	# comment
		: schema01
		| schema02
		| ...
	rule1	: schema11
		| schema12
		| ...
	rule2	: schema21
		| schema22
		| ...

Rules
	Rule definition
		. The scheme's base rule has no identifier.
		. Rule identifiers are unique combinations of digits and non-separators.

	Rule reference
		. A rule is referenced from within a schema as %rule.
		. When the schema thread position reaches that reference, then
		  the corresponding rule thread is launched (with all its schemas).
		. Built-in rules identifiers begin with a separator

Schemas
	Space Management
		. space characters are ignored after the Hyphen rule ('%-')
		. space characters are ignored before the Token sign ('%{' ), unless
		  they are following a Regex rule ('%/regex/')
		. space characters are ignored at the beginning of a schema, unless
		  this schema pertains to the base rule
		. space characters are ignored at the end of a schema
		. the Space rule ('% ') specifies that at least one space character
		  must be entered by the user. If the parser option P_SPACE is set,
		  then the spaces entered by the users are passed to the program.
		. other groups of space characters are registered in the schema as
		  a single space (' '). When the schema thread position reaches 
		  this character, then spaces entered by the user are ignored.

	Backslash Management
		. Backslash followed by carriage return allows to continue the schema
		  definition over to the next line.
		. Otherwise backslash allows to escape the special character interpretation
		  if there is
		  Example:
			: abc\ :\ def
			     ^	accept only the space character as input
				on input ' ' will move to position ':'
			: abc\t:\ def
			     ^	accept only tab character as input
				on input '\t' will move to position ':'
			: abc\%def
			     ^ accepts only '%' character as input
			     on input '%' will move to position 'd'

	Built-in rules
		. the Space built-in rule ('% ') specifies that at least one space
		  character must be entered at that position
			. Blank input characters are recorded, while
			  the schema thread remains in position
			. an input matching the character at the position immediately
			  following the '% ' rule will cause the schema thread position
			  to move to the character following that character
			. any other input will cause the schema thread to fail
		   Examples
			: abc% :def
			     ^	accept ' ', '\t' or ':' as input
			     on input ' ' or '\t' remains in position
			     on input ':'
				if there has been at least one blank input event before, then
					moves to position 'd'
						(where it does NOT accept any more space)
				otherwise
					causes the schema thread to fail
			: abc% :% def
			        ^ accepts ' ', '\t', 'd' as input
				on input ' ' or '\t' remains in position
				on input 'd'
				   if there has been at least one blank input event before, then
					moves to position 'e'
					where it doesn't accept any more space
				   otherwise
					causes the schema thread to fail

		. the Hyphen built-in rule ('%-') allows to terminate user-defined
		  rules without inserting a space
		  Examples
			: abc%rule%-def
			          ^
			: abc%rule%-\
			  def
		   Note that a space character preceding the rule '%-' will devoid
		   the rule of any effect

		. The Identifier rule ('%%') accepts any combination of digits and
		  non-separator character (at least one)
	   	  Example:
			: %%
			  ^

		. regex rules
		  Example:
			: abc%/regex/def
			     ^
		   e.g.
			: abc%/[ \t]*/def
			is the same as
				: abc def
			: abc%/[ \t]+/def
			is the same as
				: abc% def
			: abc%/[A-Za-z0-9_]+/def
			is the same as
				: abc%%def

		Note that even though the Space and Identifier rules are de facto useless
		we keep them nonetheless as implementation templates, should the need arise
		in the future for other built-in rules / shorthands

Usage
	./yak filename

 Example Scheme files
	see ./Yak/test

 API - cf main.c
	Scheme *scheme = readScheme( filename );
	Parser *parser = newParser( scheme );
	do {
		int event = input();
		switch ( ParserFrame( parser, event ) ) {
			case rvParserOutOfMemory:
				event = EOF;
				break;
			case rvParserNoMatch:
				// here all parser rules failed
				//
				ParserRebase( parser );
				break;
			case rvParserNoMore:
				// here all parser rules completed and at least one
				// base rules succeeded.
				// It is up to this implementation whether to call
				// it an error (e.g. the last event was not '\n')
				// or not
				ParserRebase( parser );
				break;
			case rvParserEncore:
				// here there are still parser rules running. we can
				// nonetheless rebase the parser, in which case it
				// will feed back results independently of their position
				// in the stream - pipeline mode. Otherwise rules will
				// not be re-activated until the longest one succeeds.
				//
				// ParserRebase( parser );
				break;
		}
	}
	while ( event != EOF );
	freeParser( parser );

Design
    Let Ri[ m ] represent the rule thread i at frame m
    Let Ri[ m ].Sj represent the schema thread j of the rule thread i at frame m

    At frame N we have
	States
		{ Ri[ N ] / i }: initially inactive, then possibly active
		{ Ri[ m ] / i, m < N }: active xor inactive, Failure
		{ Ri[ m ].Sj / <j>, Ri[ m ]: active, m < N }: active
		{ Ri[ m ].Sj.position / Ri[ m ] active, Ri[ m ].Sj: active, m < N }: k

	Events
		{ Ri[ m ] / <i>, m < N }: active xor inactive, Failure
		{ Ri[ m ].Sj / <i,j>, m < N }: active xor inactive, Success, Failure
		{ Ri[ m ].Sj.position / <i,j>, m < N }: k

	Actions
		Schema Update
                    foreach R: Ri[ m ] / m <= N, i verifies R.state: active
                        foreach S: R.Sj / j verifies S.state: active
                            in : *S.position ~: { '\0', '%' }
                                in : *S.position ~: input.event    << includes EOF >>
                                    set S.status : Failure
                                else
                                    update S.value(s), aka. register event
                                    set S.position : next( S.position )
                                    then in : *S.position : '\0'
                                        S.ACTION()
                                        set S.status : Success
        
                            // here we are pending on a rule which we activated ever since we reached
                            // that position. Either the rule failed as a whole, or some of its schemas
                            // succeeded. In the latter case we keep the rule active by forking.
        
			    // Explanation of the following formula:
			    // The rule R was launched at frame m, and its schema S is now pending on
			    // the rule Rn which was launched at frame m + p, where p is S.position -
			    // not considering the backslashes and other rule references in the position

                            else foreach Rn[ m + p ].status: Failure / n: R.reference[ j, p: S.position ]
                                set S.status : Failure
        
                            else foreach Rn[ m + p ].Sk.status: Success / n: R.reference[ j, p: S.position ], k
                                in : FORK( S ) : 0
                                    update S.value(s)
                                    set S.position : next( S.position )
                                    then in : *S.position : '\0'
                                        S.ACTION()
                                        set S.status : Success

		Rule Activation
                    // if an active Schema S reached a position n where a rule is referenced,
                    // then we need to activate that rule - and all of its schemas

	            foreach R: Ri[ m ] / m <= N, i verifies R.state: active
	                foreach S: R.Sj / j verifies R.Sj.position: p / n: R.reference[ j, p ] exists
	                    foreach Sn: Rn[ N ].Sk / k
	                        in : Sn.schema ~: ""
	                            set Rn.state : active
	                            set Sn.state : active
	                            set Sn.position : 0
	                        else    // special case: null schema
	                            in : FORK( S ) : 0
	                                set S.position : next( S.position )
	                                then in : *S.position : '\0'
	                                    // S.ACTION()
	                                    set S.status : Success

		Rule Update
                    foreach R: Ri[ m ] / m < N, i verifies R.state: active
                        set success : 0
                        then foreach S: R.Sj.status : { Failure, Success } / j
                            set S.state : inactive
                            in : S.status : Success
                                set success : 1
                            then if no R.Sk left active
                                set R.state : inactive
                                in : success : 0
                                    set R.status : Failure

	Notes
	. The base rule (rule.identifier == "") is characterized by the fact that it has no subscriber.
	  we build intermediate results. It is up to the caller to decide whether he wants to use these or not.

Implementation
	cf Yak/ source code repository

