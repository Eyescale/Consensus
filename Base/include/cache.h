#ifndef CACHE_H
#define CACHE_H

#define RECYCLE

extern int cache_count;

inline void *
allocate( void ***Cache, const int size )
{
#ifdef RECYCLE
	void **this, **cache = *Cache;
	if (( cache ))
        	this = cache;
	else {
//		fprintf( stderr, "REALLOCATING: %d\n", cache_count++ );
		this = (void **) malloc(size*2*sizeof(void*));
		this[ 1 ] = NULL;
		for ( int i=size-1; (i--); ) {
			this += 2;
			this[ 1 ] = this-2;
		}
	}
	*Cache = (void **) this[ 1 ];
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


#endif // CACHE_H
