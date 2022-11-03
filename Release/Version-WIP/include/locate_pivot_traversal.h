#ifndef LOCATE_PIVOT_TRAVERSAL_H
#define LOCATE_PIVOT_TRAVERSAL_H

static BMTraverseCB
	dot_identifier_CB, identifier_CB, character_CB, mod_character_CB,
	star_character_CB, register_variable_CB, dereference_CB, sub_expression_CB,
	dot_expression_CB, open_CB, filter_CB, decouple_CB, close_CB;
#define case_( func ) \
	} static BMCBTake func( BMTraverseData *traverse_data, char **q, int flags, int f_next ) { \
		BMLocatePivotData *data = traverse_data->user_data; char *p = *q;


#endif	// LOCATE_PIVOT_TRAVERSAL_H
