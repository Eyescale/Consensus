#include "lineno.bm"
:
	on init
		do : lc : !! LineNo(
			(l,0), (c,0), nl, (:0123456789:) )
		do : level : root
		do : cmd : cmd
		do : base
		en &
	else
		on : input : ~. // EOF
			in : ps
				do : take | (cmd,~.)
			else in : base
				in (cmd,.)
					do : err |: ErrUnexpectedEOF
				else in ready
					do : output
				else do : ~.
			else do : err |: ErrUnexpectedEOF
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
				do : cmd
			else do : err |: ErrUnexpectedSpace
		'\t'
			in ( cmd, . )
				do : cmd
			else in ~.:: tab : .
				do : level : ( *level, *^^ )
			else in ?:( *level, *^^ ) // must exist
				do : level : %?
			else do : err |: ErrIndentation
		'\n'
			in : cmd : ((type,ELSE)?( cmd ):((((.,e),l),s),e))
				in : *level : (.,(?,.))
					in ( (%?:CL) ?: (%?:DO) ?: (%?:'>') )
						do : err |: ErrElseIndentation
					else in ( (%?:(.,CL)) ?: (%?:(.,DO)) )
						do : err |: ErrRedundantElse
					else
						do : cmd : ( cmd, ~. )
						do : take // standalone ELSE
				else do : err |: ErrElseIndentation
			else in : cmd : cmd
				in ~.:: tab : .
					do : level : ( root, ~. )
				else do : level : root
			else do : err |: ErrUnexpectedCR
		'%'
			in (( *level:root )?( *cmd:cmd ):)
				// do : mode : narrative
				do : flush |: space
			else do : err | ErrUnexpectedCharacter
		'#'
			in (( *level:root )?( *cmd:cmd ):)
				do : flush
			else do : err |: ErrUnexpectedPound
		.
			do : err
	else en &

.: flush
	on : input :
		'\n'
			do ~( space )
			do : cmd : ( cmd, ~. )
			do : ( *s ? take : base )
		/[ \t]/
			do ~.
		.
			in space
				do : err |: ErrUnexpectedCharacter
	else en &

.: cmd
	in : cmd :
		((((.,e),l),s),e)
			in ( type, ELSE )
				do : err |: ErrRedundantElse
			else in : *level : (.,(?,.))
				in ( (%?:CL) ?: (%?:DO) ?: (%?:'>') )
					do : err |: ErrElseIndentation
				else in ( (%?:(.,CL)) ?: (%?:(.,DO)) )
					do : err |: ErrRedundantElse
				else
					do : cmd : ( cmd, ~. )
					do : base | ( type, ELSE )
			else do : err |: ErrElseIndentation
		((.,i),n)
			do : string | ( type, IN )
		((.,o),n)
			do : string | ( type, ON )
		(((.,o),f),f)
			do : string | ( type, OFF )
		((.,d),o)
			do : string | ( type, DO )
		((.,c ),l)
			do : string | ( type, CL )
		(.,'>')
			in ((type,ELSE)?:(~.:(*level:*tab)))
				do : err |: ErrUnexpectedStripe
			else in : *tab : (.,(?,.))
				in (( %?:DO )?:( %?:'>' ))
					do : string | ( type, '>' )
				else do : err |: ErrUnexpectedStripe
			else do : err |: ErrUnexpectedStripe
	else do : err |: ErrUnknownCommand

.: string
	in *s // started
		on : input :
			'"'
				do : ps // finish
			.
				// record string
				do : s : ( *s, *^^ )
		else en &
	else
		on : input :
			'"'
				do : s : s // start
			/[ \t]/
				do ~.
			'&'
				in ?:%( *that ? ( type, ?:~ELSE ) :)
					in (( %?:IN ) ?: ( %?:ON ) ?: ( %?:OFF ))
						do : s : ( s, *^^ )
						do : flush |: space
					else do : err
				else do : err
			.
				do : err
		else en &

.: ps
	// try to concatenate _" "_
	on : input :
		'"' // success
			do : string | { ~(nl), (ps,~.) }
		'\n'
			// backup input, starting with newline
			do ( nl | ((*,ps),((ps,*^^),~.)) )
		/[ \t]/
			do ( nl ? ((*,ps),(*ps,*^^)) :)
		.
			in : s : s // empty string
				do : err |: ErrStringEmpty
			else in nl
				// restore backup as input queue
				do : Q : ( Q | ((%|,...),< %(ps,?:...), *^^ >) )
				do : cmd : ( cmd, ~. )
				do : take 
			else in (( type, CL )?( *^^:'d' ):)
				// take CL pending DO
				do : cmd : (( cmd, ~.  ), *^^ )
				do : take
			else do : err |: ErrUnexpectedCharacter
			do { ~(nl), (ps,~.) }
	else en &

.: take
	on : .
		in (( type, '>' )?:( ~.: ready ))
			// clamp tab to level and reset level
			in ~.:: tab : *level
				do : tab : ( *level, ~. )
			in ~.: (cmd,.)
				do : level : root
		else do : output
	else
		in ( type, ?:~ELSE )
			in (( %?:CL )?:( %?:DO ))
				// in CL_DO force DO to register at same level
				// otherwise enable output - barring '>'
				do (( cmd, . ) ? ( type, ELSE ) : ready )
				in ( type, ELSE ) // ELSE CL or ELSE DO
					// **tab is informed, by preceding IN or ON or OFF
					do ( '\0' |
						?:%((?,...):*s):(.,?) (%|,%?) :| {
							((*,*tab), ( **tab, ((ELSE,%?),%|))),
							((*,that), %| ) } )
				else // CL or DO
					do ( '\0' |
						?:%((?,...):*s):(.,?) (%|,%?):| {
							((*,*tab), ((.(*tab),~.),(%?,%|))),
							((*,that), %| ) } )
			else in %?: '>' // no ELSE
				// **tab is informed, by preceding DO
				do ( '\0' |
					?:%((?,...):*s):(.,?) (%|,%?):|
						((*,*tab), (**tab, (%?,%|))) )
			else // [ ELSE ] IN, ON, OFF
				do : tab : ( *tab, '\t' ) // set new tab
				in ( s, '&' )
					do : *tab : (( type, ELSE ) ?
						( **tab, ((ELSE,%?),*that) ) :
						( .(*tab), (%?,*that) ))
				else do ( '\0' |
					?:%((?,...):*s):(.,?) (%|,%?):|
						((*,*tab), ( ( type, ELSE ) ?
							(**tab, ((ELSE,%?),%|)) :
							((.(*tab),~.),(%?,%|)) )))
		else // standalone ELSE
			do : tab : (*tab,'\t') // set new tab
			do : *tab : ( **tab, ELSE ) // inform current

		in : input : .
			do : base | { (s,~.), ~( type, ((cmd,.)?CL:.) ) }
		else do : output |: ~.

.: output
	on : r : ?
		// we have ((%?,...),(cmd,string))
		in (((%?,(CL,.)) ?: (%?,((.,CL),.))) ? ((%?,.),((.,DO),.)) :)
			do > "%sCL \"%(.,?)$\" DO \"%(.,?)$\"\n":<
				((%?,((.,.),.)) ? "ELSE " :),
				%((?,...):%(%?,(.,?)))
				%((?,...):%((%?,.),(.,?))) >
		else
			in ( %?, ?:((.,.),.))
				do >"ELSE %s \"%(.,?)$\"\n":<
					%(%?:((.,?),.)),
					%((?,...):%(%?:(.,?))) >
			else in ( %?, ?:(.,.))
				do >"%s \"%(.,?)$\"\n":<
					%(%?:(?,.)),
					%((?,...):%(%?:(.,?))) >
			else do >"ELSE\n"

			in ( ?:(%?,.), . )
				do : q : %?
			else in ?:( *p, '\t' )
				do : p : %?
	else on : q : ?
		do > "%(.,?)$": %((?,...):*p)
		do : r : %?
	else on : p : ?
		in ?:( take, %? )
			do : q : %?
	else in ready // just entered
		do >"--------------------------------------------------------\n"
		do : p : root
		do ~( ready )
	else in : output : ~.
		on ~(: take ) // just transitioned from take
			do : err |: ErrUnexpectedEOF
		else do : ~.
	else do : take

.: err
	on : . // lc coming next frame
		on ~(:?) // just transitioned from
			do >"[%_:] Error: ": %?
		else do >"Error: "
	else on ~( %%, l ) < *lc
		do >"l%$": %<(!,?:...)>
	else
		in : err : ?
			do > ": %_\n": %?
		else do >:
		do : ~.

.: &
	// post-frame narrative: read input on demand
	in : Q : ?
		in ?: ( %?, . )
			do :< input, Q >:< %?:(.,?), %? >
		else do ~( Q )
	else
		do input : "%c" <

