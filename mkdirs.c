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

    int 
mkdirs( char *path ) 
{
    char 	*p;
    int		saved_errno;
    struct stat	st;

    for ( p = path; *p == '/'; p++ )
	;
    for ( p = strchr( p, '/' ); p != NULL; p = strchr( p, '/' )) {
	*p = '\0';
	if ( mkdir( path, 0777 ) < 0 ) {
	    saved_errno = errno;
	    if ( stat( path, &st ) != 0 ) {
		errno = saved_errno;
		return( -1 );
	    }
	    if ( !S_ISDIR( st.st_mode )) {
		errno = EEXIST;
		return( -1 );
	    }
	    errno = 0;
	}
	*p++ = '/';
    }

    return( 0 );
}
