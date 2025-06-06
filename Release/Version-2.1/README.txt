Name
	Consensus Programming Language (B%) version 2.1

Usage
	./B% program.story
	./B% -p program.story
	./B% -f file.ini program.story

Description
	B% Version-2.1 aimed at completing and turning the Version-1.2
	./Examples/1_Schematize/yak.story into a complete Service Package,
	to be used by other B% applications.

	The example itself demonstrates how text input can be turned into
	externally specifiable program data & actions after segmentation,
	based on the B% capability to extract relevant sub-sequences from
	the segmentized input and pass them as arguments to appropriate
	"functions" for further processing.

	We use the term "function" here only to characterize the desired
	capability, the notion of Service Interface better representing
	both the B% execution model and Consensus's generally targeted
	client-server architecture - see ./design/story/func.story for
	a general B% Version-2.1 software architecture overview

	The NEW ./Examples/4_Yak/yak.bm service package, used by
	the NEW ./Examples/5_Dial/dpu-drive.story example, demonstrate
	successful completion of the targeted capability, relying on
	the following B% Version-2.1 New Features and Extensions

New Features and Extensions
New Features
	1. auto-deprecation of ( proxy:~%%, . ) relationship instances
	2. list operations
	3. per event < src
	4. proxies
	5. EN and sub-narrative definition

    1. auto-deprecation of ( proxy:~%%, . ) relationship instance

	B% Version-2.1 automatically deprecates ( proxy:~%%, . )
	relationship instance at the end of each frame, allowing

			do ( *server, request )
		requests by client to be received as
			on ~( %%, request ) < ?
		by server, and the corresponding
			do ( %<, request )
		responses by server to be received as
			on ~( %%, request ) < *server
		by client,

	the request and response details being transmitted together
	with the (( proxy, request ), ... ) list entity, usually
	using %<(!,?:...)> to extract individual components

    2. list operations
	    Construction
		. do (( list, ... ):sequence:)
		. do (( list, ... ), xpan )
			instantiates xpan if needed, then 
			instantiates ((((list,a),b),...),z) where xpan is {a,b,...,z}
		. do (( list, ... ),< expression(s) >)
			allows multiple expressions to be concatenated
		. do ((list,~.)|(%|,expression)) // graft
			cuts (list,.) and instantiates (list,expression)
	    Query
		. %<(!,?:...)>	// EENOV
			generally %<(_!_?_)> '?' optional '!' mandatory
		. %( list, ?:... )
			represents all list^as_sub[0]^n.sub[1] (n>=1)
		. %( list, ... )
			represents all list^as_sub[0]^n (n>=0)
		. %((?,...):list)
			represents all list.sub[0]^n (n>=0)
	    Output
		. do >"%$\n": %(list,?:...) // output concatenated
		. do >"%$\n": %<(!,?:...)>
	    Release
		. do identifier~ usage extended to support:
			locale identifier
			sub-narrative param identifier
			register variables %? or %!

		  In case identifier represents a non-base entity,
		  identifier's subs get deprecated iff
			these are not referenced elsewhere
		  	these are not proxies

    3. per event < src
	generalizes EENO on event < src
	note:	per . < .	<=> for all active cells
	   !=	per ( . ) < .	<=> for all cells on which
				    instantiation just happened
	see also CAVEATS in design/task-list.txt

    5. EN and sub-narrative definition
	. en expression <=> %( expression )
		en ~(_) <=> %( ~(_) )
	. support %% in proto and en expression
	. support ellipsis at any level in proto, e.g.
		:( .return:(( %%, .server ), .request:(.func,...) ), ... )
	. support %(_) as proto, e.g.
		.action:%( ?, Action )

	  Note that B% internally builds the string "proto:expression"
	  to query matching entities, which may influence the query's
	  pivot selection - and therefore performances

Extensions
	1. proxies
	2. assignment
	3. output
	4. parser

    1. proxies
	. now manifested upon creation
	. ( proxy:~%%, . ) relationship instances auto-deprecated
	. new register variable %@ = active connections
	. %% now traversed => in . now detects %% and (%%,.) etc.

    2. assignment
	. ASSIGNMENT RULES - REV 2.0
		%! always available when %? is, and represents
		whole expression - likewise %<!> vs. %<?>
	. vector assignment
		do :< a, b, c >:< d, e, f > // ~. supported
	  <=>
		do : a : d
		do : b : e
		do : c : f
	. self-assignment
		in/on/do/per	: expression
		    represents  : %% : expression

		%(:expression)	represents %((*,%%),expression)
		 (:expression)	represents ((*,%%),expression)
			except	in do mode where it is literal
			e.g.
				%(:?) represents current self-assignment
				on ((:state)?~(:previous):) // transition event
				do :( %(:state) ? nextA : nextB )
	. contrary assignment
		in/on ~.::variable:value
		in/on ~.::expression

    3. output
	. do >"%$\n": %(list,?:...)	// output concatenated
	. do >"format":< args >		// allows multiple args
	. do >: .			// do >:~%(.,?):~%(?,.)
	. do >:< a, b >			// separates output results by \cr
	. do >&_			// output to stderr

    4. parser
	a. now supports the following preprocessing directives
		#include "_"	// _ path, relative unless with leading /
		#define _
		#undef _
		#ifdef _
		#ifndef _
		#eldef _
		#elndef _
		#else
		#endif
	   error on \nl#identifier	// otherwise #_ considered comment
	b. in/on/per signal~ no longer supported - REV 2.0
		must use in/on/per ~( signal )
	c. expression in the following narrative occurrences can no longer
	   be marked:
		do expression
		do ~( expression )
		on ~( expression )	// not EENO
	d. :? generally not accepted - ?: has to be leading in terms
		allows ?:_ not to be considered filtered in EENO
			on ~( a, ?:b ) < .
	e. Ternary expressions
		guards not markable
		terms signalable if mono-level e.g. (_?_?a~:b~:c~)
		terms markable iff %(_?_:_) - as
			not every term may be marked
	f. inside literals and lists
	   '(' and ')' must now be backslashed, e.g.
		do (:\(whatever\):)	// required for p_prune()
	   '\'' must be backslashed unless following %identifier, in which
	        case %identifier' MUST be followed by non-separator, e.g.
		(:%titi'toto) correctly translates as ((%,titi),toto)
		(:%identifier':) or (:%identifier') generates error
	g. do : proxy : !! identifier
		same as do : proxy : !!identifier()
	h. \n accepted inside
		{_} <_> and carry(_) 
	   as well as after
		(_? and (_?_:
	i. locale declaration allowed as part of ELSE narrative occurrences,
	   e.g.
		else .locale
	   same as
		else
			.locale
		



