/*
 * Copyright (c) 2003 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>

#include "mkdirs.h"

/*
 * The right most element of the path is assumed to be a file.
 */
    int 
mkdirs( char *path ) 
{
    char 	*p, *q = NULL;
    int		saved_errno;

    saved_errno = errno;

    /* try making longest path first, working backward */
    for (;;) {
	if (( p = strrchr( path, '/' )) == NULL ) {
	    errno = EINVAL;
	    return( -1 );
	}
	*p = '\0';
	if ( q != NULL ) {
	    *q = '/';
	}

	if ( mkdir( path, 0777 ) == 0 ) {
	    break;
	}
	if ( errno != ENOENT ) {
	    return( -1 );
	}
	q = p;
    }

    *p = '/';

    if ( q != NULL ) {
	p++;
	for ( p = strchr( p, '/' ); p != NULL; p = strchr( p, '/' )) {
	    *p = '\0';
	    if ( mkdir( path, 0777 ) < 0 ) {
		return( -1 );
	    }
	    *p++ = '/';
	}
    }

    errno = saved_errno;

    return( 0 );
}
