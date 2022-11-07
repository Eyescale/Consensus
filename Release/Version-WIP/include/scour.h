#ifndef SCOUR_H
#define SCOUR_H

int bm_scour( char *expression, int target );

#define EMARK		1
#define QMARK		2
#define PMARK		4
#define	PARENT		8
#define PERSO		16
#define IDENTIFIER	32
#define CHARACTER	64
#define	MOD		128
#define	STAR		256
#define	SELF		512
#define	EENOK		1024


#endif	// SCOUR_H
