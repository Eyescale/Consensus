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
				do : s : $((s,~.),?:...)
				in ( type, '>' )
					do : cmd : ( cmd, ~. )
					do : ward
				else
					do : ((type,CL) ? ward : take )
					in ~.: ready
						do : cmd : ( cmd, ~. )
			else in ((:base)?(~.:(cmd,.)):)
				// ready => warding has been performed
				do : ( ready ? inform : ~. )
			else do : err
		else on : ~.
			do :< guard, on, bar >: ~.
			do : report
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
				// this is where we know whether DO is generative or not
				do : ( ((type,CL)?:(type,'>')?:(*^^:'>')) ? ward : take )
			else in (( type, CL )?( *^^:'d' ):)
				// take CL pending DO
				do :< cmd, s >:< ((cmd,~.),*^^), $((s,~.),?:...) >
				do : ward
			else do : err
			do ~( ps )
	else en &

.: ward
	in ( type, CL )
		in ((?:!!,DO), *s )
			do : err |: ( NCR, %? )
		else in ((?:!!,CL), *s )
			do : take |: %?
		else do : take |: !! | {
			((%|,IN), $(line,?:...)),
			((%|,CL), *s ) }
	else in ( type, DO ) // assumed generative
		in ((?:!!,CL), *s )
			do : err |: ( NCR, %? )
		else in ((?:!!,'>'), *s )
			do : err |: ( NCR, %? )
		else in ((?:!!,DO), *s )
			do : take |: %?
		else do : take |: !! | {
			((%|,IN), $(line,?:...)),
			((%|,DO), *s ) }
	else // ( type, '>' )
		in ((?:!!,DO), *s )
			do : err |: ( NCR, %? )
		else do : take |
			((*take,'>'), *s )

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

.: report
	on : .
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

#		per ?:%(?:~ELSE,/(ON|OFF)/):~%((!!,'>'),?):~%(.,((.,.),(?,ON)))
		per ?:%(?:~ELSE,/(ON|OFF)/):~%((!!,'>'),?):~%(.,((.,.),(?,.)))
			do >&"Warning: ErrUnspecifiedOrigin: \"%s\"\n": %?
			do : err |: ErrSystemIncomplete
	else
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

