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
				do : (((type,CL)?:(type,'>')) ? ward : take )
				do : s : $((s,~.),?:...)
				do : cmd : ( cmd, ~. )
			else in : ((cmd,.)?~.: base )
				/* ready is set by :take upon completion of
					[ELSE] CL ELSE DO
					[ELSE] CL
					[ELSE] DO
				   and kept until the next, not '>' command
				   => warding has been performed
				*/
				do : ( ready ? inform : ~. )
			else do : err
		else on : ~.
			do exit
		else en %(:?)

.: base
	on : input :
		/[inofelscd>]/
			in ~.:: tab : .
				do : tab : *level
			do : cmd : ( *cmd, *^^ )
		' '
			in ( cmd, . )
				do : check
			else do : err
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
		do : base
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
				do : err
			else in : *tab : (.,(?,.))
				in (( %?:DO )?:( %?:'>' ))
					do : line | ( type, '>' )
				else do : err
			else do : err
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
					do : s : '&'
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
			do : cmd : ( cmd, ~. )
		.
			do : err
	else en &

.: ps
	// try to concatenate _" "_
	on : input :
		'"' // success
			do : string | { ~( ps ) }
		'\n'
			// backup starting with newline
			do : ps : (( ps, *^^ ), ~. )
		/[ \t]/
			in : ps : ?
				do : ps : ( %?, *^^ )
		.
			in : s : s // empty string
				do : err |: ErrStringEmpty
			else in : ps : .
				// restore backup as input queue
				do : buffer : ( buffer | ((%|,...),< %(ps,?:...), *^^ >) )
				// this is where we know whether DO is generative or not
				do : ( ((type,CL)?:(type,'>')?:(*^^:'>')) ? ward : take )
				do : s : $((s,~.),?:...)
				do : cmd : ( cmd, ~. )
			else in (( type, CL )?( *^^:'d' ):)
				// take CL pending DO
				do : cmd : ((cmd,~.), *^^ )
				do : s : $((s,~.),?:...)
				do : ward
			else do : err
			do ~( ps )
	else en &

.: ward
	in ( type, DO ) // assumed generative
		in ((?:!!,CL), *s )
			do : err |: ( NCR, %? )
		else in ((?:!!,'>'), *s )
			do : err |: ( NCR, %? )
		else in ((?:!!,DO), *s )
			do : take |: %?
		else do : take |: !! | {
			((%|,IN), $(line,?:...)),
			((%|,DO), *s ) }
	else in ( type, CL )
		in ((?:!!,DO), *s )
			do : err |: ( NCR, %? )
		in ((?:!!,CL), *s )
			do : take |: %?
		else do : take |: !! | {
			((%|,IN), $(line,?:...)),
			((%|,CL), *s ) }
	else // ( type, '>' )
		in ((?:!!,DO), *s )
			do : err |: ( NCR, %? )
		else do : take

.: take
	on : .
		in (( type, '>' )?:( ~.: ready ))
			// clamp tab to level and reset level
			in ~.:: tab : *level
				do : tab : ( *level, ~. )
			in ~.: (cmd,.)
				do : level : root
		else do : inform
	else
		in ( type, ?:~ELSE )
			in (( %?:CL )?:( %?:DO ))
				/* in pending DO force DO to register at same level,
				   otherwise set ready */
				do ((cmd,.) ? (type,ELSE) : ready )

				/* in (type,ELSE) **tab is informed, by preceding
				   IN or ON or OFF */
				do :< *tab, that >:< ((type,ELSE) ?
					( **tab, ((ELSE,%?),*s) ) :
					((.(*tab),~.),(%?,*s))), *s >
			else in %?: '>' // no ELSE
				// **tab is informed, by preceding DO
				do : *tab : ( **tab, (%?,*s) )
			else // [ ELSE ] IN, ON, OFF
				do : tab : ( *tab, '\t' ) // set new tab
				do : *tab : (( type, ELSE ) ?
					( **tab, ((ELSE,%?),((*s:'&')?*that:*s)) ) :
					( (.(*tab),~.), (%?,((*s:'&')?*that:*s) )))
		else // standalone ELSE
			do : tab : (*tab,'\t') // set new tab
			do : *tab : ( **tab, ELSE ) // inform current

		in : input : .
			do : base | { (s,~.), ~((cmd,.)?(type,CL):(type,.)) }
		else do : inform

.: inform
	on : r : ?
		do (( *take, '>' ), %(%?,(.,?)))
		in ( ?:(%?,.), . )
			do : r : %?
	else on : q : ?
		in ( %?, (DO,?))
			do : action : (%?,ON)
			in ( ?:(%?,.), . ) // generated occurrence
				do : r : %?
		else in ( %?, (CL,?))
			do : action : (((%?,.),.) ? // CL-DO
				((%?,OFF), (%((%?,.),(.,?)),ON)) :
				(%?,OFF))
		else
			in : condition : ?
				do (( %?, OFF ), *guard )
			in : event : ?
				do ( %?, *bar )

			in ( %?, ((.,DO),?))
				do :< condition, event >: ~.
				do : action : (%?,ON)
			else in ( %?, ((.,CL),?))
				do :< condition, event >: ~.
				do : action : (((%?,.),.) ? // ELSE CL-DO
					((%?,OFF), (%((%?,.),(.,?)),ON)) :
					(%?,OFF))
			else
				in ?:( (%?,(IN,.)) ?: (%?,((.,IN),.)) )
					do :< condition, event >:< %?:(.,(.,?)), ~. >
				else in ?:( (%?,(ON,.)) ?: (%?,((.,ON),.)) )
					do :< condition, event >:< ~., (%?:(.,(.,?)),ON) >
				else in ?:( (%?,(OFF,.)) ?: (%?,((.,OFF),.)) )
					do :< condition, event >:< ~., (%?:(.,(.,?)),OFF) >
				else // standalone ELSE
					do :< condition, event >: ~.

				in ( ?:(%?,.), . )
					do : q : %?
				else in ?:( *p, '\t' )
					do : p : %?
	else on : p : ?
		in ?:( take, %? )
			in : event : ?
				do ( %?, *on )
				do : event : ~.
			in : condition : ?
				do (( %?, ON ), *guard )
				do : condition : ~.
			do : q : %?
		else in ?:( %?, '\t' )
			do : p : %?
	else in ready
		do :< condition, event >: ~.
		do :< guard, bar, on >: !!
		do : p : root
		do ~( ready )
	else on : . // just entered, from take, not ready
		do : err
	else in : action : .
		do : register
	else do : err

.: register
/*
	try and reuse existing guard and triggers bar/on, from
	   ( .guard, ( .trigger:(.bar,.on), .action:(.occurrence,ON|OFF) ))
	and instantiate result
*/
	on : .
		per ?:%( ?:~%(~%(?,*guard),?), ( ., (.,/(ON|OFF)/) ))
			in ~.:( ~%(?,%?), *guard ) // all conditions apply
				do { ~(*guard), ((*,guard),%?) }

		per ?:%( ., ((?:~%(~%(?,*bar),?),.), (.,/(ON|OFF)/) ))
			in ~.:( ~%(?,%?), *bar ) // all events apply
				do { ~(*bar), ((*,bar),%?) }

		in ?:%( ., ((.,?:~%(~%(?,*on),?)), (.,/(ON|OFF)/) ))
			in ~.:( ~%(?,%?), *on ) // all events apply
				do { ~(*on), ((*,on),%?) }
	else
		do ( *guard, ((*bar,*on), ( *action:((.,OFF),(.,ON)) ?
			{ %(*action:(?,.)), %(*action:(.,?)) } :
			*action )))
		do :( *input ? take :)

.: err
	on : . // lc coming up next
		on ~(:?) // just transitioned from
			do >"[%_:] Error: ": %?
	else on ~( %%, l ) < *lc
		do >"l%$": %<(!,?:...)>
	else
		in : err :
			(NCR,?)
				do >": ErrGenerativeVsGenerated, re. l%$\n": %((%?,IN),?)
			.
				do > ": %_\n": %?
		else in : input :
			'\t'
				do >": ErrUnexpectedTab\n"
			'\n'
				do >": ErrUnexpectedCR\n"
			~.
				do >": ErrUnexpectedEOF\n"
			.
				do >": ErrUnexpectedCharacter: '%s'\n": *^^
		do : ~.

.: &
	// post-frame narrative: read input on demand
	in : buffer : ?
		in ?: ( %?, . )
			do :< input, buffer >:< %?:(.,?), %? >
		else do ~( buffer )
	else
		do input : "%c" <

