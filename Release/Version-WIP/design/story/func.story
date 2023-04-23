/*===========================================================================
// Client definition - ASYNC
:
	on init
		do : server : !! Server( )
	else
		en ~%(?,.):(( %%, %@ ), ... ) // enable client request handlers

		// build request - aka. do < variables >:< server.func( params ) &
		do (((( %%, *server ),(( func, ... ),< params >)),...),< variables >)

// Client definition - SYNC
:
	on init
		do : server : !! Server( )
	else in ~.: ( %%, %@ ) // SYNC
		// build request - aka. do < variables >:< server.func( params )
		do (((( %%, *server ),(( func, ... ),< params >)),...),< variables >)
	else
		en ~%(?,.):(( %%, %@ ), ... ) // enable client request handlers

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
: Server
	%( func, ... )
	per ~( %%, ?:(func,...) ) < .
		in %<?:(((func,.),.),.)>	// verifies number of args
			do (( %%, %< ), %<?> )
		else
			// TBD: can fill in with default values
			// or return request
			// or ...

// func implementation
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

===========================================================================*/

	on init
		do (((( a, b ),(( c, ... ), d )),...), e )
	else
		do >: ~%(?,.):~%(.,?)
		do exit

