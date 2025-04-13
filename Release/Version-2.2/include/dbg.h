#ifndef DBG_H
#define DBG_H

#ifdef MAIN_H

int dbg_frame = 0;

#else // !MAIN_H
#ifdef TRACE

extern int dbg_frame;

#define DBG_OPERATE_BGN { \
	if ( !narrative->proto ) \
		printf( "===================================================[ %d ]\n", dbg_frame++ ); \
	else printf( "--------------------------------------------------- %s\n", narrative->proto ); }
#define DBG_IN_CONDITION_BGN \
	printf( "( %s, \"%s\", ctx, mark )\n", (flags&AS_PER?"IN_X":"IN"), expression );
#define DBG_IN_CASE_BGN { \
	char *s = type==IN_L ? "IN_L" :
		  type==ON_L ? "ON_L" :
		  type==L_IN ? "L_IN" : "L_ON";
	printf( "( %s, \"%s\", ctx, mark )\n", s, expression ); }
#define DBG_ON_EVENT_BGN \
	printf( "( %s, \"%s\", ctx, mark )\n", "ON", expression );
#define DBG_ON_EVENT_X_BGN \
	printf( "( %s, \"%s\", ctx, mark )\n", (as_per?"PER_X":"ON_X"), expression );
#define DBG_DO_ACTION_BGN \
	printf( "( %s, \"%s\", ctx, story )\n", "DO", expression );
#define DBG_DO_ENABLE \
	printf( "( %s, \"%s\", ctx, narratives, story, subs )\n", "EN", en );
#define DBG_DO_ENABLE_X \
	printf( "( %s, \"%s\", ctx, narratives, story, subs )\n", "EN_X", en );
#define DBG_DO_INPUT_BGN \
	printf( "( %s, \"%s\", ctx )\n", "INPUT", expression );
#define DBG_DO_OUTPUT_BGN \
	printf( "( %s, \"%s\", ctx )\n", "OUTPUT", expression );
#define DBG_SET_LOCALE \
	printf( "( %s, \"%s\", ctx )\n", "LOCALE", expression );

#define DBG_OPERATE_END
#define DBG_IN_CONDITION_END
#define DBG_ON_EVENT_END
#define DBG_ON_EVENT_X_END
#define DBG_DO_ACTION_END
#define DBG_DO_INPUT_END
#define DBG_DO_OUTPUT_END

#else // !TRACE
#ifdef DEBUG

#define DBG_OPERATE_BGN \
	fprintf( stderr, "operate bgn\n" );
#define DBG_IN_CONDITION_BGN \
	if ( flags&AS_PER ) fprintf( stderr, "in_condition bgn: per %s\n", expression ); \
	else fprintf( stderr, "in_condition bgn: in %s\n", expression );
#define DBG_IN_CASE_BGN \
	fprintf( stderr, "in_case bgn: %s\n", expression );
#define DBG_ON_EVENT_BGN \
	fprintf( stderr, "on_event bgn: %s\n", expression );
#define DBG_ON_EVENT_X_BGN \
	if ( as_per ) fprintf( stderr, "on_event_x bgn: per %s\n", expression ); \
	else fprintf( stderr, "on_event_x bgn: on %s\n", expression );
#define DBG_DO_ACTION_BGN \
	fprintf( stderr, "do_action bgn: %s\n", expression );
#define DBG_DO_ENABLE \
	fprintf( stderr, "do_enable bgn: %s\n", en );
#define DBG_DO_ENABLE_X \
	fprintf( stderr, "do_enable_x bgn: %s\n", en );
#define DBG_DO_INPUT_BGN \
	fprintf( stderr, "do_input bgn: %s\n", expression );
#define DBG_DO_OUTPUT_BGN \
	fprintf( stderr, "do_output bgn: %s\n", expression );
#define DBG_SET_LOCALE \
	fprintf( stderr, "set_locale bgn: %s\n", expression );
#define DBG_PASS_THROUGH \
	fprintf( stderr, "do_enable: built '%s'\n", q ); \
	if ( !strncmp( q, "%(.,...):", 9 ) ) \
		errout( OperationProtoPassThrough, p );

#define DBG_OPERATE_END		fprintf( stderr, "operate end\n" );
#define DBG_IN_CONDITION_END	fprintf( stderr, "in/per condition end\n" );
#define DBG_IN_CASE_END		fprintf( stderr, "in case end\n" );
#define DBG_ON_EVENT_END	fprintf( stderr, "on event end\n" );
#define DBG_ON_EVENT_X_END	fprintf( stderr, "on_event_x end\n" );
#define DBG_DO_ACTION_END	fprintf( stderr, "do_action end\n" );
#define DBG_DO_INPUT_END	fprintf( stderr, "do_input end\n" );
#define DBG_DO_OUTPUT_END	fprintf( stderr, "do_input end\n" );

#else	// !DEBUG

#define DBG_OPERATE_BGN
#define DBG_IN_CONDITION_BGN
#define DBG_IN_CASE_BGN
#define DBG_ON_EVENT_BGN
#define DBG_ON_EVENT_X_BGN
#define DBG_DO_ACTION_BGN
#define DBG_DO_ENABLE
#define DBG_DO_ENABLE_X
#define DBG_DO_INPUT_BGN
#define DBG_DO_OUTPUT_BGN
#define DBG_SET_LOCALE
#define DBG_PASS_THROUGH

#define DBG_OPERATE_END	
#define DBG_IN_CONDITION_END
#define DBG_IN_CASE_END
#define DBG_ON_EVENT_END
#define DBG_ON_EVENT_X_END
#define DBG_DO_ACTION_END
#define DBG_DO_INPUT_END
#define DBG_DO_OUTPUT_END

#endif	// DEBUG
#endif	// TRACE
#endif	// MAIN_H

#endif	// DBG_H
