#ifndef SCOUR_TRAVERSAL_H
#define SCOUR_TRAVERSAL_H

static BMTraverseCB
	sc_dot_expr_CB, sc_identifier_CB, sc_star_character_CB,
	sc_mod_character_CB, sc_character_CB, sc_register_variable_CB;
#define case_( func ) \
	} static BMCBTake func( BMTraverseData *traverse_data, char **q, int flags, int f_next ) { \
		BMScourData *data = traverse_data->user_data; char *p = *q;


#endif // SCOUR_TRAVERSAL_H
