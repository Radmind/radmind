/*
 * Copyright (c) 1998 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

/*
 * Return parsed argc/argv from the net.
 */

#include <sys/param.h>
#include <stdlib.h>

#include "argcargv.h"

#define ACV_ARGC		10
#define ACV_WHITE		0
#define ACV_WORD		1
static unsigned	acv_argc;
static char	**acv_argv;

    int
argcargv( char *line, char **argv[] )
{
    int		ac;
    int		state;

    if ( acv_argv == NULL ) {
	if (( acv_argv =
		(char **)malloc( sizeof( char *) * ACV_ARGC )) == NULL ) {
	    return( -1 );
	}
	acv_argc = ACV_ARGC;
    }

    ac = 0;
    state = ACV_WHITE;

    for ( ; *line != '\0'; line++ ) {
	switch ( *line ) {
	case ' ' :
	case '\t' :
	case '\n' :
	    if ( state == ACV_WORD ) {
		*line = '\0';
		state = ACV_WHITE;
	    }
	    break;
	default :
	    if ( state == ACV_WHITE ) {
		acv_argv[ ac++ ] = line;
		if ( ac >= acv_argc ) {
		    /* realloc */
		    if (( acv_argv = (char **)realloc( acv_argv,
			    sizeof( char * ) * ( acv_argc + ACV_ARGC )))
			    == NULL ) {
			return( -1 );
		    }
		    acv_argc += ACV_ARGC;
		}
		state = ACV_WORD;
	    }
	}
    }

    acv_argv[ ac ] = NULL; 
    *argv = acv_argv;
    return( ac );
}
