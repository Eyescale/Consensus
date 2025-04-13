#include "include/lineno.bm"
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
				do : take | (cmd,~.)
			else in : ((cmd,.)?~.: base )
				do : ( ready ? output : ~. )
			else do : err
		else on : ~.
			do exit
		else en %(:?)

.: base
	on : input :
		/[inofelscd>]/
			in ((cmd,' ')?( *^^:'>' ):( *^^:~'>' ))
				in ~.:: tab : .
					do : tab : *level
				do : cmd : ( *cmd, *^^ )
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
				do : err
			else in : *tab : (.,(?,.))
				in (( %?:DO )?:( %?:'>' ))
					do : string | ( type, '>' )
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

.: string
	in : s : ? // started
		on : input :
			'"'
				do : ps // unquote
			.
				// record string
				do : s : ( %?, *^^ )
		else en &
	else
		on : input :
			'"'
				do : s : s // quote
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
			do : string | { ~(ps) }
		'\n'
			// backup start on newline
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
				do : cmd : ( cmd, ~. )
				do : take 
			else in (( type, CL )?( *^^:'d' ):)
				// take CL pending DO
				do : cmd : (( cmd, ~.  ), *^^ )
				do : take
			else do : err
			do ~( ps )
	else en &

.: take
	on : .
		in ready
			in ( type, '>' ) // no ELSE
				// **tab is informed, by preceding DO
				in : s : '&'
					do : *tab : ( **tab, (%?,'&') )
				else do ( '\0' |
					?:%((?,...):*s):(.,?) (%|,%?):|
						((*,*tab), (**tab, (%?,%|)) ))
				do : level : root
				do : base | { ((*,s),~.), ~(type,.) }
			else do : output
		else
			// clamp tab to level and reset level
			in ~.:: tab : *level
				do : tab : ( *level, ~. )
			in ~.: (cmd,.)
				do : level : root
	else
		in ( type, ?:~ELSE )
+		in (( %?:CL )?:( %?:DO ))
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
		else // [ ELSE ] IN, ON, OFF
			do : tab : ( *tab, '\t' ) // set new tab
			in : s : '&'
				do : *tab : (( type, ELSE ) ?
					( **tab, ((ELSE,%?),*that) ) :
					( .(*tab), (%?,*that) ))
			else do ( '\0' |
				?:%((?,...):*s):(.,?) (%|,%?):|
					((*,*tab), ( ( type, ELSE ) ?
						(**tab, ((ELSE,%?),%|)) :
						((.(*tab),~.),(%?,%|)) )))
-		else // standalone ELSE
			do : tab : (*tab,'\t') // set new tab
			do : *tab : ( **tab, ELSE ) // inform current

		in : input : .
			do : base | { ((*,s),~.), ~((cmd,.)?(type,CL):(type,.)) }
		else do : output

.: output
	on : r : ?
		// we have ((%?,...),(cmd,string))
		in (((%?,(CL,.)) ?: (%?,((.,CL),.))) ? ((%?,.),((.,DO),.)) :)
			do > "%sCL \"%$\" DO \"%$\"\n":<
				((%?,((.,.),.)) ? "ELSE " :),
				%((?,...):%(%?,(.,?))),
				%((?,...):%((%?,.),(.,?))) >
		else
			in ( %?, ?:((.,.),.))
				do >"ELSE %s \"%$\"\n":<
					%?::((.,?),.),
					%((?,...):%?::(.,?)) >
			else in ( %?, ?:(.,.))
				in %?:('>','&')
					do >"> &\n"
				else do >"%s \"%$\"\n":<
					%?::(?,.),
					%((?,...):%?::(.,?)) >
			else do >"ELSE\n"

			in ( ?:(%?,.), . )
				do : q : %?
			else in ?:( *p, '\t' )
				do : p : %?
	else on : q : ?
		do > "%$": %((?,...):*p)
		do : r : %?
	else on : p : ?
		in ?:( take, %? )
			do : q : %?
		else in ?:( %?, '\t' )
			do : p : %?
	else in ready
		do >"--------------------------------------------------------\n"
		do : p : root
		do ~( ready )
	else on : . // just entered, from take, not ready
		do : err
	else in : input : .
		do : take
	else do : ~.

.: err
	on : . // lc coming up next
		on ~(:?) // just transitioned from
			do >"[%_:] Error: ": %?
	else on ~( %%, l ) < *lc
		do >"l%$": %<(!,?:...)>
	else
		in : err : ?
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
			do :< input, buffer >:< %?::(.,?), %? >
		else do ~( buffer )
	else
		do input : "%c" <

