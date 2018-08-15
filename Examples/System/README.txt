
Name
	./Examples/System

Overview
	The purpose of Consensus is to allow to create Systems of information which are
	sensitive, that is: which subscribe to and rely upon each other's changes
	to inform their own states, as opposed to trying to change the other Systems'
	states to have them align with and preserve their own.

	We believe the sensitive approach to be, not only the only sensible approach
	to represent and understand the way Systems work in reality, but also the only
	approach that gives us the chance to harmonize ourselves with our environment.

	The example source code under the ./Examples/System directory illustrates how
	such systems can be architected using linear programming language such as C.

Design
	. A System is entirely defined by the series of its States.
	. The State of a System is the Vector of States of its Subsystems.
	. Events and Actions are State changes, i.e.
		event = S:a->b	where S is a System and a, b two of its States
		action = S:a->b	where S is a System and a, b two of its States
	. The General State Change Propagation Equation is
		[ S: a->b ]=>[ S': a'->b' ]
		where S and S' are two Cosystems, and a, b, a', b' their States.
	  Note: within the same system (S = S' above), the GSCPE translates as
		The Action exerted on a System is the second order derivative of
		the series of its States - cf. Second Law of Motion (Newton)
	. A program - aka. System's Narrative - takes the following form:

		in current State: Condition1
			on change [ from previous ] to current State: Event1
				do change [ from current ] to next State: Action1

		in current State: Condition2
			on change [ from previous ] to current State: Event2
				do change [ from current ] to next State: Action2

		etc.

	Where
	. a State represents the contents of [a portion of] the memory, whose
	  differential is tracked at each program step (frame)
	. external events are mapped into internal events - System's WorldView

Usage
	./system

Execution
	. The program will read anything from stdin and
		  1. output "hello, world\n" upon carriage return
		  2. exit upon '/' followed by carriage return

Description
	. Implements CNSystem execution model and interfaces - including
		. Subsystem class definition and interfaces
	  	. CNIn, CNOn, CNDo API - in include/system.h
	. Implements Input and Parser subsystems, executing in parallel

Contents
	. main.c
		System definition - CNSystemBegin() / CNSystemEnd()
		. declares Operator system together with Parser and Input subsystems

		Subsystem definition - via subsystems ID and init() method
			. invokes each subsystem's init() method
			. init() assigns other subsystem's methods

		System execution - SystemFrame()
		. invokes each subsystem's frame() method
			. allows subsytems to subscribe to each other data changes
			. allows each subsystem to process changes from last frame
		. invokes each subsystem's sync() method
			. allows each subsystem to map update notifications into
			  its own worldview and log changes for the next frame
		. swaps the back and front buffers of each subsystem
			. these buffers hold the subsystem's change information, ie.
			  the memory addresses which were accessed via CNDo

		System exit - SystemFrame()
		. SystemFrame() returns 0 when all subsystems' frontbuffer are empty
		  ie. when no subsystem has any event to process
		. otherwise, SystemFrame() returns 1

	. Input.c
	  Parser.c
		Subsystem implementations - data and methods
		. SubsystemInit() - allocate subsystem-specific data and methods
		. SubsystemFrame() - processes changes from last frame
		. SubsystemSync() - map external change notifications into own
		  Subsystem's worldview and log local changes for the next frame

	. include/Input.h
	  include/Parser.h
		. Subsystem manifests - system interface and data structures
		  access to data structure is needed for other subsystems to subscribe

Dependencies
	./Library/include/system.h
		. System interface definition
			. Subsystem class definition
			. CNIn, CNOn, CNDo implementation - uses macros
	./Library/libcn.so
		. Consensus library

