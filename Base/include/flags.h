#ifndef FLAGS_H
#define FLAGS_H

#define are_f( b ) \
	((flags&(b))==(b))
#define is_f( b ) \
	( flags & (b) )
#define f_set( b ) \
	flags |= (b);
#define f_clr( b ) \
	flags &= ~(b);
#define f_xor( b ) \
	flags ^= (b);
#define f_reset( b, save ) \
	flags = (b)|(flags&(save));
#define	f_push( stack ) \
	add_item( stack, flags );
#define	f_pop( stack, save ) \
	flags = pop_item( stack )|(flags&(save));


#endif	// FLAGS_H
