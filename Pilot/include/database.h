#ifndef DATABASE_H
#define DATABASE_H

typedef struct _Entity
{
	// entity data
        struct _Entity *sub[3];
	int twist, state;

	// memory management
        struct _Entity *next;
	listItem *as_sub[3];
}
Entity;

Entity *newEntity( Entity *source, Entity *medium, Entity *target, int pointback );
void freeEntity( Entity *this );

#endif	// DATABASE_H
