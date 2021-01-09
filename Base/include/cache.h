#ifndef CACHE_H
#define CACHE_H
#ifdef CACHE_C

void **	UCache = 0;
int	UCount = 0;

#else // not CACHE_C

extern void **UCache;
extern int UCount;

#define RECYCLE
#define UCACHE
#define C 1500

inline void *
allocate( void ***Cache )
{
#ifdef RECYCLE
	void **this, **cache = *Cache;
	if (( cache ))
        	this = cache;
	else {
//		fprintf( stderr, "B%%: allocate: %d\n", UCount++ );
		this = malloc(C*2*sizeof(void*));
		this[ 1 ] = NULL;
		for ( int i=C-1; (i--); ) {
			this += 2;
			this[ 1 ] = this-2;
		}
	}
 	*Cache = this[ 1 ];
	return this;
#else
	return malloc(2*sizeof(void*));
#endif
}

inline void
recycle( void **item, void ***Cache )
{
#ifdef RECYCLE
	item[ 1 ] = *Cache;
	*Cache = item;
#else
	if ( item == NULL ) {
		fprintf( stderr, "***** Warning: recycle(): NULL\n" );
		return;
	}
	item[ 0 ] = NULL;
	item[ 1 ] = NULL;
	free((void *) item );
#endif
}

#endif // CACHE_C
#endif // CACHE_H
