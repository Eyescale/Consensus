#ifndef PROXY_H
#define	PROXY_H

typedef struct {
	BMContext *ctx;
	struct { listItem *flags, *x; } stack;
	CNDB *db_x;
	CNInstance *x;
} BMProxyFeelData;

#endif	// PROXY_H
