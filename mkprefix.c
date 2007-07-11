/*
 * Copyright (c) 2003 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/param.h>

#include "mkprefix.h"

int extern	quiet;
int extern	showprogress;

/* mkprefix attempts to create intermediate directories of path.
 * Intermediate directories are created with the permission of the
 * mode and UID of the last pre-existing parent directory. 
 */

    int 
mkprefix( char *path ) 
{
    char 	*p, parent_path[ MAXPATHLEN * 2 ];
    int		saved_errno, parent_stats = 0;
    uid_t	e_uid;
    struct stat	st, parent_st;
    mode_t	mode = 0777;

    e_uid = geteuid();

    /* Move past any leading /'s */
    for ( p = path; *p == '/'; p++ )
	;

    /* Attempt to create each intermediate directory of path */
    for ( p = strchr( p, '/' ); p != NULL; p = strchr( p, '/' )) {
	*p = '\0';
	if ( mkdir( path, mode ) < 0 ) {
	    /* Only error if path exists and it's not a directory */
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
	    *p++ = '/';
	    continue;
	}

	/* Get stats from parent of first missing directory */
	if ( !parent_stats ) {
	    if ( snprintf( parent_path, MAXPATHLEN, "%s/..", path)
		    > MAXPATHLEN ) {
		fprintf( stderr, "%s/..: path too long\n", path );
		*p++ = '/';
		return( -1 );
	    }
	    if ( stat( parent_path, &parent_st ) != 0 ) {
		return( -1 );
	    }
	    parent_stats = 1;
	}

	/* Set mode to that of last preexisting parent */
	if ( mode != parent_st.st_mode ) {
	    if ( chmod( path, parent_st.st_mode ) != 0 ) {
		return( -1 );
	    }
	}

	/* Set uid to that of last preexisting parent */
	if ( e_uid != parent_st.st_uid ) {
	    if ( chown( path, parent_st.st_uid, parent_st.st_gid ) != 0 ) {
		return( -1 );
	    }
	}

	if ( !quiet && !showprogress ) {
	    printf( "%s: created missing prefix\n", path );
	}

	*p++ = '/';
    }
    return( 0 );
}
