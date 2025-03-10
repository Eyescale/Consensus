#include "lineno.bm"
:
	on init
		do : lc : !! LineNo( (:0123456789:) )
		do : level : root
		do : cmd : cmd
		do : base
		en &
	else
		on : input : ~. // EOF
			in : ps
				do :< cmd, s >:< (cmd,~.), $((s,~.),?:...) >
				do : ward
			else in ((:base)?(~.:(cmd,.)):)
				// ready => warding has been performed
				do : ( ready ? inform : ~. )
			else do : err
		else on : ~.
			do { ~( root ), ~( /(ELSE|ON|OFF|CL|DO|>)/, . ) }
			do :< guard, on, bar >: ~.
			do : verify
		else en %(:?)

.: base
	on : input :
		/[inofelscd>]/
			in ((cmd,' ')?( *^^:'>' ):( *^^:~'>' ))
				do : cmd : ( *cmd, *^^ )
				in ~.:: tab : .
					do : tab : *level
			else do : err
		' '
			in ( cmd, . )
				do : check
			else do : cmd : ( cmd, *^^ )
		'\t'
			in ( cmd, . )
				do : check
			else in ~.:: tab : .
				do : level : ( *level, '\t' )
			else in ?:( *level, '\t' ) // must exist
				do : level : %?
			else do : err
		'\n'
			in ( type, ~ELSE )
				do : err
			else in : cmd : ((((.,e),l),s),e)
				do : check // standalone ELSE
			else in ~.:(cmd,.)
				in ( type, . )
					do : check // standalone ELSE
				else // empty line
					do : level : (*tab?root:(root,~.))
			else do : err
		'#'
			in (( *level:root )?(~.:(cmd,.)):)
				do : flush
			else do : err
		.
			do : err
	else en &

.: flush
	on : input : '\n'
		do :( ps ?: base )
	else en &

.: check
	in : cmd :
		((((.,e),l),s),e)
			in ( type, ELSE )
				do : err |: ErrRedundantElse
			else
				do ( type, ELSE )
				do : cmd : ( cmd, ~. )
				do : ( *input:'\n' ? . : base )
		((.,i),n)
			do : line | ( type, IN )
		((.,o),n)
			do : line | ( type, ON )
		(((.,o),f),f)
			do : line | ( type, OFF )
		((.,d),o)
			do : line | ( type, DO )
		((.,c ),l)
			do : line | ( type, CL )
		(.,'>')
			in ((type,ELSE)?:(~.:(*level:*tab)))
				do : err |: ErrStripeCommand
			else in : *tab : (.,(?,.))
				in (( %?:DO )?:( %?:'>' ))
					do : line | ( type, '>' )
				else do : err |: ErrStripeCommand
			else do : err |: ErrStripeCommand
		^^
			// standalone ELSE
			in : *level : (.,(?,.))
				in ( (%?:CL) ?: (%?:DO) ?: (%?:'>') )
					do : err |: ErrIndentation
				else in ( (%?:(.,CL)) ?: (%?:(.,DO)) )
					do : err |: ErrRedundantElse
				else do : take
			else
				do : err |: ErrIndentation
		.
			do : err |: ErrUnknownCommand

.: line
	on ~( %%, l ) < ?:*lc
		do : string | (((line,~.),...),%<(!,?:...)>)

.: string
	in : s : ? // started
		on : input :
			'"' // unquote
				do : ps
			.
				do : s : ( %?, *^^ ) // record
		else en &
	else
		on : input :
			'"' // quote
				do : s : s
			/[ \t]/
				do ~.
			'&'
				in ( *that ? (~.:((type,CL)?:(type,DO))) :)
					do : s : *that
					do : finish
				else do : err
			.
				do : err
		else en &

.: finish
	on : input :
		/[ \t]/
			do ~.
		'\n'
			do : take
			do : cmd : (cmd,~.)
			in (type,'>')
				do ((*take,'>'),'&')
		.
			do : err
	else en &

.: ps
	// try to concatenate _" "_
	on : input :
		'"' // success
			do : string | {~(ps)}
		'\n'
			// backup starting with newline
			do : ps : (( ps, ~. ), *^^ )
		/[ \t]/
			in : ps : ?
				do : ps : ( %?, *^^ )
		'#'
			in ( ps, '\n' )
				do : flush
			else do : err
		.
			in : s : s // empty string
				do : err |: ErrStringEmpty
			else in : ps : .
				// restore backup as input queue
				do : buffer : ( buffer | ((%|,...),< %(ps,?:...), *^^ >) )
				do :< cmd, s >:< (cmd,~.), $((s,~.),?:...) >
				do : ward
			else in (( type, CL )?( *^^:'d' ):)
				// take CL pending DO
				do :< cmd, s >:< ((cmd,~.),*^^), $((s,~.),?:...) >
				do : ward
			else do : err
			do ~( ps )
	else en &

.: ward
	in ( type, '>' )
		in ((?:!!,/(DO|CL|SET)/), *s )
			do : err |: ( NCR, %? )
		else do : take |
			((*take,'>'), *s )
	else in ( type, CL )
		in ((?:!!,'>'), *s )
			do : err |: ( NCR, %? )
		else in ((?:!!,DO), *s )
			do : err |: ( NCR, %? )
		else in ((?:!!,CL), *s )
			do : take
		else do : take |: !! | {
			((%|,IN), $(line,?:...)),
			((%|,CL), *s ) }
	else in ( type, DO )
		in ((?:!!,'>'), *s )
			do : err |: ( NCR, %? )
		else in ~.:: input : '>'
			in ((?:!!,/(DO|CL|SET)/), *s )
				do : take
			else do : take |: !! | {
				((%|,IN), $(line,?:...)),
				((%|,SET), *s ) }
		else // generative DO
			in ((?:!!,CL), *s )
				do : err |: ( NCR, %? )
			else in ((?:!!,DO), *s )
				do : take |: %?
			else in ((?:!!, SET), *s )
				do (((%?,IN),~.), $(line,?:...))
				do { ~(%?,SET), ((%?,DO), *s ) }
				do : take |: %?
			else do : take |: !! | {
				((%|,IN), $(line,?:...)),
				((%|,DO), *s ) }
	else do : take

.: take
	on : .
		in ready
			in ((type,'>') ? *input :)
				do : level : root
				do : base | { ((*,s),~.), ~(type,.) }
			else do : inform
		else
			// clamp tab to level and reset level
			in ~.:: tab : *level
				do : tab : ( *level, ~. )
			in ~.: (cmd,.)
				do : level : root
	else
		in ?:(( type, CL )?:( type, DO ))
			in ((cmd,.) ? *input :) // CL->DO
				do ( type, ELSE )
			else do ready
			do : *tab : (( type, ELSE ) ?
				( **tab, ((ELSE,%?::(.,?)),*s) ) :
				( (.(*tab),~.), (%?::(.,?),*s) ))
			do : that : *s
		else in ( type, ?:~ELSE )
			do : tab : ( *tab, '\t' )
			do : *tab : (( type, ELSE ) ?
				( **tab, ((ELSE,%?),*s) ) :
				( (.(*tab),~.), (%?,*s) ))
		else // standalone ELSE
			do : tab : (*tab,'\t') // set new tab
			do : *tab : ( **tab, ELSE ) // inform current

		in *input
			do : base | { ((*,s),~.), ~((cmd,.)?(type,CL):(type,.)) }
		else do : inform

.: inform
	on : q : ?
		in ( %?, (DO,?))
			do : action : (%?,ON)
		else in ( %?, (CL,.))
			do : action : (((%?,.),.) ? // CL-DO
				((%(%?,(.,?)),OFF), (%((%?,.),(.,?)),ON)) :
				(%(%?,(.,?)),OFF))
		else
			in : condition : ?
				do (( %?, OFF ), *guard )
			in : event : ?
				do ( %?, *bar )

			in ( %?, ((.,DO),?))
				do : action : (%?,ON)
			else in ( %?, ((.,CL),.))
				do : action : (((%?,.),.) ? // ELSE CL-DO
					((%(%?,(.,?)),OFF), (%((%?,.),(.,?)),ON)) :
					(%(%?,(.,?)),OFF))
			else
				in ?:( %(%?,(IN,?)) ?: %(%?,((.,IN),?)) )
					do : condition : %?
					do : event : ~.
				else in ?:( %(%?,(ON,?)) ?: %(%?,((.,ON),?)) )
					do : condition : ~.
					do : event : ( %?, ON )
				else in ?:( %(%?,(OFF,?)) ?: %(%?,((.,OFF),?)) )
					do : condition : ~.
					do : event : ( %?, OFF )
				else // standalone ELSE
					do :< condition, event >: ~.

				in ( ?:(%?,.), . )
					do : q : %?
				else in ?:( *p, '\t' )
					do : p : %?
	else on : p : ?
		in ?:( take, %? )
			do : q : %?
			// register last condition/event from previous level
			in : condition : ?
				do (( %?, ON ), *guard )
				do : condition : ~.
			in : event : ?
				do ( %?, *on )
				do : event : ~.
		else in ?:( %?, '\t' )
			do : p : %?
	else in ready
		do :< guard, on, bar >: !!
		do : p : root
		do ~( ready )
	else on : . // just entered, from take, not ready
		do : err
	else in : action : .
		do :< condition, event >: ~.
		do : instantiate
	else do : err |: ErrNoAction

.: instantiate
	on : .
		// try to reuse existing guard and triggers { on, bar }
		per ?:%( ?:~%(~%(?,*guard),?), ((!!,!!), (.,/(ON|OFF)/) ))
			in ~.:( ~%(?,%?):~(*,.), *guard ) // all *guard conditions apply
				do { ~(*guard), ((*,guard),%?) }

		per ?:%( !!, ((?:~%(~%(?,*on),?),!!), (.,/(ON|OFF)/) ))
			in ~.:( ~%(?,%?):~(*,.), *on ) // all *on events apply
				do { ~(*on), ((*,on),%?) }

		per ?:%( !!, ((!!,?:~%(~%(?,*bar):~!!,?)), (.,/(ON|OFF)/) ))
			in ~.:( ~%(?,%?):~(*,.), *bar ) // all *bar events apply
				do { ~(*bar), ((*,bar),%?) }
	else
		// instantiate ( *guard, ((*on,*bar), *action[s] ))
		in : action : ?
			in %?: ((.,OFF),(.,ON))
				do ( *guard, ((*on,*bar), { %?::(?,.), %?::(.,?) } ))
				do ~( %? )
			else do ( *guard, ((*on,*bar), %? ))

		in : input : .
			do : take
		else in ( cmd, . )
			do : cmd : (cmd,~.)
			do : take
		else do : ~.

#define UNGEN	~%((!!,'>'),?):~%!/((?,DO),!):%((?,'>'),'&')/
#define REPORT
.: verify
	on : .
		in ?: "init"
			in (((!!,DO),%?) ?: ((!!,'>'),%?))
				do : err |: ErrInitUsage
			else in ( ?:(%?,ON), . )
				in ~.:( ., %? )
					do : init : %?
				else do : err |: ErrInitUsage
			else do : err |: ErrNoInit
		else do : err |: ErrNoInit
	else on : init : .
		in ?: "exit"
			in (((!!,DO),%?) ?: ((!!,'>'),%?))
				do : err
			else in ( ., ?:(%?,ON))
				in ~.:( %?, . )
					do : exit : %?
				else do : err |: ErrExitUsage
			else do : err |: ErrNoExit
		else do : err |: ErrNoExit
	else on : exit : .
		in ?:%((?,ON):~*init):~%(.,((.,.),(?,ON))):UNGEN
			do >&"Warning: ErrNoActionGenerating: \"%s\" ON\n": %?
			do : err |: ErrSystemIncomplete
		else in ?:%(?,OFF):~%(.,((.,.),(?,OFF))):UNGEN
			do >&"Warning: ErrNoActionGenerating: \"%s\" OFF\n": %?
			do : err |: ErrSystemIncomplete
#ifdef REPORT
	else do : report
#else
	else do : launch
#endif

.: report
	per .action: %( ., ((.,.), ?:(.,/(ON|OFF)/)))
		do >": \"%_\" %_\n":< %(action:(?,.)), %(action:(.,?)) >
		in ((?:!!,DO), %(action:(?,.))) // generative action
			per ((%?,'>'),?:~'&') // generated occurrences
				do >"  > \"%_\"\n": %?
			in ((%?,'>'),'&')
				do >"  > &\n"
		per .guard: %( ?, ((.,.), action ))
			do >"  guard\n"
			per ( ?, guard ) // guard condition
				do >"\t\"%_\" %_\n":< %?::(?,.), %?::(.,?) >
			per ( guard, ( ?, action ))
				do >"    trigger\n"
				per ( ?, %?::(?,.) ) // on event
					do >"\t\"%_\" %_\n":< %?::(?,.), %?::(.,?) >
				do >"      /\n"
				per ( ?:~!!, %?::(.,?) ) // bar event
					do >"\t\"%_\" %_\n":< %?::(?,.), %?::(.,?) >
		do >:
	do exit

#define OCC	( %!/((?,.),!):%((.,CO),^^)/, . )
#define COS	%!/((?,CO),!):%((?,.),%?::(?,.))/
#define GEN	%!/((?,'>'),!):%((?,DO),%?::(.,(?,.)))/
.: launch
	on : .	// ascribe occurrence to cosystem
		per .occurrence: %( ?, /(ON|OFF)/ )
			in (( ?:!!, DO ), occurrence )
				do (( %?, CO ), ?::occurrence )
			else in (( !!, '>' ), occurrence )
				// done as per DO
			else do (( %?, CO ), system )
	else in (?:!!,CO):~%(?,.)
		do >&"Warning: origin not found: \"%s\"\n": %((%?,DO),?)
		do : err |: ErrSystemIncomplete
	else
		do :%((.,CO),?): !! (
			?:(*init:OCC)(%?,*^^),
			?:(*exit:~OCC)(%?,*^COS),
			?:((!!,!!),OCC){(%?)|{
				?:(GEN)(%?,{ON,OFF}),
				?:(?,%?::((?,.),.))((%?,*^COS),%|::((?,.),.)),
				?:(?:~!!,%?::((.,?),.))((%?,*^COS),%|::((.,?),.)),
				?:(?:!!,%?){(%?,%|)|
					?:(?,%?)((%?,*^COS), %|::(?,.))}}} )
		do exit

.: err
	on : . // lc coming up next
		on ~(:?) // just transitioned from
			do >&"[%_:] Error: ": %?
	else on ~( %%, l ) < *lc
		do >&"l%$": %<(!,?:...)>
	else
		in : err :
			(NCR,?)
				do >&": generative vs. generated usage: "
					"prior reference in l%$\n": %((%?,IN),?)
			.
				do >&": %_\n": *^^
		else in : input :
			'\t'
				do >&": ErrUnexpectedTab\n"
			'\n'
				do >&": ErrUnexpectedCR\n"
			~.
				do >&": ErrUnexpectedEOF\n"
			.
				do >&": ErrUnexpectedCharacter: '%s'\n": *^^
		do exit

.: &
	// post-frame narrative: read input on demand
	in : buffer : ?
		in ?: ( %?, . )
			do :< input, buffer >:< %?::(.,?), %? >
		else do ~( buffer )
	else
		do input : "%c" <

