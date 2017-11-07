#ifndef CGI_H
#define CGI_H

/*---------------------------------------------------------------------------
	actions		- public
---------------------------------------------------------------------------*/

_action read_cgi_output_session;
_action cgi_scheme_op_pipe;
_action read_cgi_input;

/*---------------------------------------------------------------------------
	utilities	- public
---------------------------------------------------------------------------*/

int cgi_output_session( char *session );

#endif	// CGI_H
