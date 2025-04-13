Name
	Consensus Programming Language (B%) version 2.2

Usage
	./B% program.story
	./B% -p program.story
	./B% -f file.ini program.story
	./B% -f file.ini -p

	./B% -i
	./B% file.bm -i
	./B% -f file.ini -i
	./B% -f file.ini file.bm -i

Description
	B% Version-2.2 delivers the functionality necessary to support the design
	specified in ./design/story/interactive.story, whereby a System is defined
	as a set of relationship instances ( .guard, ( .trigger, .action ) )

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
		event: (( occurrence, /(ON|OFF)/ ), cosystem )
		condition: (( occurrence, /(ON|OFF)/ ), cosystem )

	A final, functional implementation of this design can be found under
	./Examples/6_System/ which features the three increasingly complete
	stories

		./Examples/6_System/output.story
		./Examples/6_System/inform.story
		./Examples/6_System/system.story

	all reading the externally specified ./Examples/6_System/TM.system


New Features and Additions

	 1. forescan / instantiation loop
	 2. filter-pipe operation
	 3. piped assignment
	 4. switch/case assignment evaluation
	 5. strings and unnamed base entities (UBE)
	 6. assignment register variables and buffer operation
	 7. selection operation
	 8. string creation
	 9. lists and tagging operation
	10. query identifier variable
	11. iterator list and pop-list assignment
	12. register variable ::sub operation
	13. string narrative class origin
	14. EEVA - external event value assignment
	15. narrative subclassing and post-frame narrative
	16. function narrative

    1. forescan / instantiation loop
		do ?:forescan(_)
		do ?:forescan{_}
	where
		forescan is any queryable expression starting with %( or (
		and finishing with )

    2. filter-pipe operation
		(_):|
	replaces or sets current pipe register variable %|'s value
	with expression result - as per instantiation
	Example usage
		do ( expr1 | expr2 :| expr3 )
		do ( expression | ?:(_)(_):| )
		do ( ?:(_)(_):| expression )

    3. piped assignment
		do : expression | expression
		do : expression |: expression

    4. switch/case assignment evaluation
		in/on : switch-expression :
			case-expression
					do action
			case-expression
			case-expression
					do action
	where
		register variables ^^ and *^^ hold, respectively
		the first switch- and case- expression result

    5. strings and unnamed base entities (UBE)
	strings - can be used in the following instances
		do "string"
		do : expression : "string"
		in/on ?: "string"
			...
		: "string"
			// string narrative
	UBE - can be used in the following instances
		do !! | { expressions invoving %| }
		do : expression(s) : !! { expressions involving %| }
		do : expression(s) : !!
	!! can also be used in queries to specify a type requirement

    6. assignment register variables and buffer operation
	The register variables ^^ and *^^ can be used in the following
	carry- and ube- expressions
		do : variable : !! class( carry-expression(s) )
		do : variable : !! ( carry-expression(s) )
		do : variable : !! { ube-expression(s) }
	where
		The register variable ^^ represents the current variable
		The register variable *^^ represent the associated proxy
		resp. ube
		the dereferencing operator *^ allows to dereference
		the operand in assignment buffer - e.g.
			*^%?
			*^%!
			*^%!/selection/
		so that, for instance, *^^ equals *^%!/(!:^^)/

    7. selection operation
	typically associated with unnamed base entity creation, allows to
	return a specific field value using UBE as pivot - somewhat similar
	to a Database SELECT operation, e.g.

	%!/((?:!!,field2),!):%((?,field0),value0):%((?,field1),value1)/
		returns field2's value of the ube created via
			do !! | {
				((%|,field0),value2),
				((%|,field1),value1),
				((%|,field2),value2) }
	Generally
			%!/(_?_!_):%(_):%(_):.../
		is the same as
				%(_%(_):%(_)_?_)
	expression created by replacing ! in the original expression with ?
	and by replacing ? in the original expression by the filtering terms

	NOTE: embedded selection is not allowed - cf parser.c:f_selectable()

    8. string creation
		do : string : $((s,~.),?:...)
		do : string : $(s,?:...)
	where s is an identifier verifying e.g.
		((((s,a),b),....),z) => the above create
			: string : "ab...z"
		with or without deprecating (s,.)

    9. lists and tagging operation
		.%list: < expression(s) >
		.%list: { expression }	// traverses list
		.%list~: { expression }	// traverses list, popping
		.%list~
	where
		list is an identifier
		^. represents current list item, to be kept in list
		^.~ represents current list item, to be removed from list

		%( expression|^tag )
		%( expression|^tag~ )
	where
		tag is an identifier
		^tag means "add to 'tag' list"
		^tag~ means "remove from 'tag' list"

	Note: see below post-frame narrative section: post-frame narrative
	allows once-per-frame final list update, after all the tagging is done

   10. query identifier variable
		in/on/per .identifier: expression
			...
	where
		identifier gets pushed as a locale variable

   11. iterator list and pop-list assignment
		else on ...
			in .%iterator: expression
				do : variable : ^iterator~
			else do : variable : ~.
		else on : variable : ?
			...
			do : variable : ^iterator~
		else on : variable : ~.
			...
	where
		iterator is a list identifier

	Note: the following syntax may also be used
		in ?:^iterator~
			do : variable : %?
		else ...

   12. register variable ::sub operation
		%?::sub is the same as %(%?:sub)
		%!::sub is the same as %(%!:sub)
		%|::sub
	where
		sub is a valid (_) expression whose terms consist
		solely of the wildcards . or ? and where (.,.) is
		not allowed

   13. string narrative class origin
		in .string: "string"
			do : classname : ?::string::default
	assigns to variable the name of the first class where "string"
	narrative has been defined, default if none exist

   14. EEVA - external event value assignment
	%<::> or %<.> as term of a query expression passes if the
	corresponding match's sub-entity is either
	. of type (( variable, value ), proxy ) and verifies either
		on : variable : value < proxy
	  or, if this fails and in the case of %<.>, verifies
		on ~((( variable, value ), proxy ), . )
	. of type ( variable, value ) and verifies
		on : variable : value

   15. narrative subclassing and post-frame narrative
	: < class	// main subclass definition
	: name < class	// subclass definition
	.: identifier	// identifier subnarrative
	.: &		// post-frame narrative

	base class supercedes sub-class main and post-frame narratives
	no 'en' command is allowed in post-frame narrative

   16. function narrative
		.: function ( .args )
	where
		function is an identifier

Misc.
	. regex group /(option|option|...)/ now supported
	. do { ..., ~( expression ), ... } allows release action
	. do > "%$": %((?,...):*p) same as
	  do > "%$": %(%((?,...):*p):(.,?))
	. do >"%s\n": "hello, world" supported
	. on ~(:?) assigned previous self-assignment to %?
	. in (~.:condition) supported as contrary condition
	. do ~( %% ) => exits


