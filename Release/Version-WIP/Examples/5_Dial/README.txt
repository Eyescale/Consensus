Name
	Consensus/Release/Version-WIP/Examples/5_Dial/dpu-drive.story

Purpose
	A calculator - aka. dial processing unit - using pre-instantiated dial
	to perform basic arithmetic operations, and demonstrating

	A complete yak-based B% story implementation using multiple external
	B% service packages - here ../4_Yak/yak.bm and dpu.bm - to process
	user input and interaction

Package dependencies
	carry.story
	cmp.story
	dpu-add.story
		< dpu.bm
			< dial.bm
	dpu-drive-ui.story
		< ../4_Yak/yak.bm
	dpu-drive.story
		< ../4_Yak/yak.bm
		< dpu.bm
			< dial.bm
	mult-table.story
		< dial.bm
	sub.story

About dpu-drive.story
    DPU interface
	do ( *dpu, GET )
		on ~( %%, GET ) < ?:*dpu
			do (((*A,*), ... ), %<(!,?:...)> )
	do ((( *dpu, SET ), ... ), digit(s) )
		on ~( %%, SET ) // sync - optional
	do ((( *dpu, ADD ), ... ), digit(s) )
		on ~( %%, ADD ) // sync
			do (((*A,*), ... ), %<(!,?:...)> )
	do ((( *dpu, MULT ), ... ), digit(s) )
		on ~( %%, MULT ) // sync
			do (((*A,*), ... ), %<(!,?:...)> )

    Accumulator operations
	1. push
		in : A : ? // current accumulator address
			do : A : ((A,%?)|((%|,%?),*op))
			do : op : new_op

		The current accumulator address is *A
		The next accumulator address is (A,*A)
		Note: the current op is pushed along as
			  (( (A,*A), *A), *op )
		      so as to be performed on pop
	2. pop
		in : A : ( A, ?:~A ) // previous accumulator address
			do :< A, op >:< %?, %((*A,%?),?) >
			do ~( A, %? )
	3. assignment
		in ( *A, * )
			do : *A : ( **A, digit )
		else	do : *A : (( *A, * ), digit )
	   <=>
		in : A : ? // current accumulator address
			do : %? : ((%?,*) ? (*%?,%<?>) : ((%?,*),%<?>))

		The accumulator value is **A: (((*A,*), ... ), digits )

About dpu.bm
    Perform number operations - using dial.bm for single-digit operations

About dial.bm
    Perform single-digit operations - interface example in mult-table.story

    MULT Optimization
	Asumming that
		1. p or q in { 0, 1, base-1 } are dealt with separately
		2. base is even (e.g. 10 or 16)

	We want to take advantage of the known optimized way to compute
		(base/2) x q and (base-1) x q
	to compute p x q optimally - vs. adding q to itself (p-1) times

	e.g. in	    	    base 10		    base 16
		p: 0	return 0		return 0
		p: 1	return q		return q
		p: 2	return q+q		return q+q
		p: 3	return q+q+q		return q+q+q
		p: 4	return 5q-q		return q+q+q+q
		p: 5	return 5q		return 8q-q-q-q
		p: 6	return 5q+q		return 8q-q-q
		p: 7	return 9q-q-q		return 8q-q
		p: 8	return 9q-q		return 8q
		p: 9	return 9q		return 8q+q
		p: A				return 8q+q+q
		p: B				return 8q+q+q+q
		p: C				return Fq-q-q-q
		p: D				return Fq-q-q
		p: E				return Fq-q
		p: F				return Fq

    Algorithm
	eval n=neg(p)
			0	       	    p	   base
			x - - - - - - + - - + - - - x
			  i->         X         <-n

		n=neg(p) is met when i==p
		may or may not have crossed X=base/2

	if n equals i equals p then
		compute X x q, i.e.
			alternate 0|p q times, incrementing report every 2nd time
	else if i crossed X=base/2
		if [ X, p ] < [ p, base ]
			compute p x q as: X x q + [ X, p ] x q
		else
			sub q from (base-1)*q, neg(p)-1 times
	else // n did not cross X
		if [ 0, p ] < [ p, X ]
			add q to p, p-1 times
		else
			compute p x q as: X x q - [ p, X ] x q

    Results - as per mult-table.story

  0   0    0    0    0    0    0    0    0    0    0    0    0    0    0   0 
  0   1    2    3    4    5    6    7    8    9    A    B    C    D    E   F 
  0   2 |  4 |  6 |  8 |  A |  C |  E | 10 | 12 | 14 | 16 | 18 | 1A | 1C  1E 
  0   3 |  6 |  9 |  C |  F | 12 | 15 | 18 | 1B | 1E | 21 | 24 | 27 | 2A  2D 
  0   4 |  8 |  C | 10 | 14 | 18 | 1C | 20 | 24 | 28 | 2C | 30 | 34 | 38  3C 
  0   5 !  A !  F ! 14 ! 19 ! 1E ! 23 ! 28 ! 2D ! 32 ! 37 ! 3C ! 41 ! 46  4B 
  0   6 !  C ! 12 ! 18 ! 1E ! 24 ! 2A ! 30 ! 36 ! 3C ! 42 ! 48 ! 4E ! 54  5A 
  0   7 !  E ! 15 ! 1C ! 23 ! 2A ! 31 ! 38 ! 3F ! 46 ! 4D ! 54 ! 5B ! 62  69 
  0   8 . 10 . 18 . 20 . 28 . 30 . 38 . 40 . 48 . 50 . 58 . 60 . 68 . 70  78 
  0   9 - 12 - 1B - 24 - 2D - 36 - 3F - 48 - 51 - 5A - 63 - 6C - 75 - 7E  87 
  0   A - 14 - 1E - 28 - 32 - 3C - 46 - 50 - 5A - 64 - 6E - 78 - 82 - 8C  96 
  0   B - 16 - 21 - 2C - 37 - 42 - 4D - 58 - 63 - 6E - 79 - 84 - 8F - 9A  A5 
  0   C _ 18 _ 24 _ 30 _ 3C _ 48 _ 54 _ 60 _ 6C _ 78 _ 84 _ 90 _ 9C _ A8  B4 
  0   D _ 1A _ 27 _ 34 _ 41 _ 4E _ 5B _ 68 _ 75 _ 82 _ 8F _ 9C _ A9 _ B6  C3 
  0   E _ 1C _ 2A _ 38 _ 46 _ 54 _ 62 _ 70 _ 7E _ 8C _ 9A _ A8 _ B6 _ C4  D2 
  0   F   1E   2D   3C   4B   5A   69   78   87   96   A5   B4   C3   D2  E1 

