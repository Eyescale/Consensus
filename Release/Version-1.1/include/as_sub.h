#ifndef AS_SUB_H
#define AS_SUB_H

#ifndef OPT //=============== as_sub operations NOT optimized ===============

#define add_as_sub( instance, as_sub, p ) addItem( as_sub, instance )
#define remove_as_sub( instance, as_sub, p ) removeItem( as_sub, instance )

inline CNInstance *
lookup_as_sub( CNInstance *e, CNInstance *f, const int pivot )
{
	switch ( pivot ) {
	case 0:
		for ( listItem *i=f->as_sub[ 1 ]; i!=NULL; i=i->next ) {
			CNInstance *instance = i->ptr;
			if ( instance->sub[ 0 ] == e )
				return instance;
		}
		break;
	default:
		for ( listItem *i=e->as_sub[ 0 ]; i!=NULL; i=i->next ) {
			CNInstance *instance = i->ptr;
			if ( instance->sub[ 1 ] == f )
				return instance;
		}
	}
	return NULL;
}

#else //=============== as_sub operations optimized ===============
#ifdef DEBUG
#define BREAK \
	else if ( !compare ) { \
		fprintf( stderr, ">>>>> B%%: Error: add_as_sub(): already there\n" ); \
		return; \
	} break;
#else
#define BREAK break;
#endif

inline void
add_as_sub( CNInstance *instance, listItem **as_sub, const int p )
{
	listItem *i, *last_i=NULL, *j=newItem( instance );
	switch ( p ) {
	case 0:
		instance = instance->sub[ 1 ];
		for ( i=*as_sub; i!=NULL; i=i->next ) {
			CNInstance *g = i->ptr;
			intptr_t compare = (uintptr_t) g->sub[ 1 ] - (uintptr_t) instance;
			if ( compare < 0 ) { last_i=i; continue; }
			BREAK
		}
		break;
	default:
		instance = instance->sub[ 0 ];
		for ( i=*as_sub; i!=NULL; i=i->next ) {
			CNInstance *g = i->ptr;
			intptr_t compare = (uintptr_t) g->sub[ 0 ] - (uintptr_t) instance;
			if ( compare < 0 ) { last_i=i; continue; }
			BREAK
		}
	}
	if (( last_i )) { last_i->next = j; j->next = i; }
	else { j->next = *as_sub; *as_sub = j; }
}

inline void
remove_as_sub( CNInstance *instance, listItem **as_sub, const int p )
{
	CNInstance *h;
	listItem *i, *last_i;
	switch ( p ) {
	case 0:
#ifdef UNIFIED
		if ( !(h=instance->sub[ 1 ]) || !instance->sub[0] ) {
#else
		if ( !(h=instance->sub[ 1 ]) ) {
#endif
			removeItem( as_sub, instance );
			return;
		}
		for ( i=*as_sub, last_i=NULL; i!=NULL; i=i->next ) {
			CNInstance *g = i->ptr;
#ifdef UNIFIED
			if ( !g->sub[0] ) { last_i=i; continue; }
#endif
			intptr_t compare = (uintptr_t) g->sub[ 1 ] - (uintptr_t) h;
			if ( compare < 0 ) { last_i=i; continue; }
			else if ( !compare ) {
				if (( last_i )) last_i->next = i->next;
				else *as_sub = i->next;
				freeItem( i );
			}
			return;
		}
		break;
	default:
		if ( !(h=instance->sub[ 0 ]) ) {
			removeItem( as_sub, instance );
			return;
		}
		for ( i=*as_sub, last_i=NULL; i!=NULL; i=i->next ) {
			CNInstance *g=i->ptr, *k=g->sub[ 0 ];
#ifdef UNIFIED
			if ( !k ) { last_i=i; continue; }
#endif
			intptr_t compare = (uintptr_t) k - (uintptr_t) h;
			if ( compare < 0 ) { last_i=i; continue; }
			else if ( !compare ) {
				if (( last_i )) last_i->next = i->next;
				else *as_sub = i->next;
				freeItem( i );
			}
			return;
		}
	}
}

inline CNInstance *
lookup_as_sub( CNInstance *e, CNInstance *f, const int p )
{
	switch ( p ) {
	case 0:
		for ( listItem *i=f->as_sub[ 1 ]; i!=NULL; i=i->next ) {
			CNInstance *instance = i->ptr;
			intptr_t comparison = (uintptr_t) instance->sub[ 0 ] - (uintptr_t) e;
			if ( comparison < 0 ) continue;
			else if ( comparison ) break;
			return instance;
		}
		break;
	default:
		for ( listItem *i=e->as_sub[ 0 ]; i!=NULL; i=i->next ) {
			CNInstance *instance = i->ptr;
			intptr_t comparison = (uintptr_t) instance->sub[ 1 ] - (uintptr_t) f;
			if ( comparison < 0 ) continue;
			else if ( comparison ) break;
			return instance;
		}
	}
	return NULL;
}

#endif // OPT =========================================================

#endif // AS_SUB_H
