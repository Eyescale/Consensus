/*===========================================================================
|
|    B% Version-2.1 software architecture & New Features design overview
|
+==========================================================================*/

//---------------------------------------------------------------------------
// Client definition
//---------------------------------------------------------------------------
:
	on init
		do : server : !! Server( )
	else
#ifdef ASYNC
		/* enable client request handlers
		*/
		en (( %%, %@ ), ... ):~%(?,.)

		/* build request - could be built-in as
			do < variables >:< server.func( params ) &	// ASYNC
			do < variables >:< server.func( params )	// SYNC
		*/
		do (((( %%, *server ),(( func, ... ),< params >)),...),< variables >)
#else
		in ( %%, %@ ) // SYNC
			en (( %%, %@ ), ... ):~%(?,.)
		else do (((( %%, *server ),(( func, ... ),< params >)),...),< variables >)
#endif

// Client request handler
:( .return:(( %%, .server ), .request:((.func,...)), ... )
	// Note: due to varargs,
	//	.locale(s) will be allocated as ((%%,this),locale)
	//	.( expression ) will be interpreted as ((%%,this),expression)
	on ( this )
		do ( server, request )		// send (auto-deprecate)
	else on ~( %%, request ) < server	// receive
		do :%( return, ?:... ):%<(!,?:...)>
		do return~


//---------------------------------------------------------------------------
// Server definition
//---------------------------------------------------------------------------
: Server
	%( func, ... )
	per ~( %%, ?:(func,...) ) < .
		in %<?:(((func,.),.),.)>	// verifies number of args
			do (( %%, %< ), %<?> )
		else
			// TBD: can fill in with default values
			// or return request
			// or ...

// Server func - aka. Service - implementation
:((( func, .arg1 ), .arg2 ), .arg3 )

	// return results - ideally:
	do ((( %((%%,?),this), this ), ... ), < results > )

	// but this assumes the client passed the proper number of args
	// so: send the same < results > to clients with best-matching
	// args, whether:

	// 1. client's request has the complete set of this args (and possibly more)
	do ( %((%%,.), %(this,...)) |
		((( %(%|:((.,?),.)), %(%|:(.,?))), ... ), <results> ) )

	// 2. client's request only has a subset of this args
	do ( %((%%,.), %((?,...):%((?,.):this))) |
		((( %(%|:((.,?),.)), %(%|:(.,?))), ... ), <results> ) )

	do this~

