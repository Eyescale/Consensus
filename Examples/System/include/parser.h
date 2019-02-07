#ifndef CN_PARSER_H
#define CN_PARSER_H

typedef struct {
	struct {
		int input;
	} call;
	struct {
		int event;
	} input;
	int state;
}
ParserData;

#endif	// CN_PARSER_H
