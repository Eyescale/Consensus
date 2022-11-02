#ifndef SCOUR_TRAVERSAL_H
#define SCOUR_TRAVERSAL_H

static BMTraverseCB
	dot_expr_CB, identifier_CB, star_character_CB,
	mod_character_CB, character_CB, register_variable_CB;
#define case_( func ) \
	} static BMCBTake func( BMTraverseData *traverse_data, char **q, int flags, int f_next ) { \
		BMScourData *data = traverse_data->user_data; char *p = *q;


#endif // SCOUR_TRAVERSAL_H
