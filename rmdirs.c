/*
 * Copyright (c) 2003 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <errno.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "rmdirs.h"

    int
rmdirs( char *path )
{
    int			i, len, unlinkedfiles;
    char		temp[ MAXPATHLEN ];
    DIR			*dir;
    struct dirent	*dirent;
    struct stat		st;

    if (( dir = opendir( path )) == NULL ) {
	return( -1 );
    }

    /* readdir() on HFS+ is broken:
     * http://docs.info.apple.com/article.html?artnum=107884
     *
     * "The unspecified behavior is what readdir() should return after the
     * directory has been modified. Many file systems have been implemented
     * such that subsequent readdir() calls will return the next directory
     * entry. The implementation of the HFS file system cannot guarantee
     * that all enclosed files or directories will be removed using the
     * above method."
     */

    do {
	unlinkedfiles = 0;

	while (( dirent = readdir( dir )) != NULL ) {

	    /* don't include . and .. */
	    if (( strcmp( dirent->d_name, "." ) == 0 ) ||
		    ( strcmp( dirent->d_name, ".." ) == 0 )) {
		continue;
	    } 

	    len = strlen( path );

	    /* absolute pathname. add 2 for / and NULL termination.  */
	    if (( len + strlen( dirent->d_name ) + 2 ) > MAXPATHLEN ) {
		fprintf( stderr, "Absolute pathname too long\n" );
		goto error;
	    }
	
	    if ( path[ len - 1 ] == '/' ) {
		if ( snprintf( temp, MAXPATHLEN, "%s%s", path, dirent->d_name )
			>= MAXPATHLEN ) {
		    fprintf( stderr, "%s%s: path too long\n", path,
			dirent->d_name );
		    goto error;
		}           
	    } else {
		if ( snprintf( temp, MAXPATHLEN, "%s/%s", path, dirent->d_name )
			>= MAXPATHLEN ) {
		    fprintf( stderr, "%s/%s: path too long\n", path,
			dirent->d_name );
		    goto error;
		}
	    }

	    if ( lstat( temp, &st ) != 0 ) {
		/* XXX - how to return path that gave error? */
		fprintf( stderr, "%s: %s\n", temp, strerror( errno ));
		goto error;
	    }
	    if ( S_ISDIR( st.st_mode )) {
		if ( rmdirs( temp ) != 0 ) {
		    fprintf( stderr, "%s: %s\n", temp, strerror( errno ));
		    goto error;
		}
	    } else {
		if ( unlink( temp ) != 0 ) {
		    fprintf( stderr, "%s: %s\n", temp, strerror( errno ));
		    goto error;
		}
		unlinkedfiles = 1;
	    }
	    if ( unlinkedfiles ) {
		rewinddir( dir );
	    }
	}
    } while ( unlinkedfiles );

    if ( closedir( dir ) != 0 ) {
	return( -1 );
    }
    if ( rmdir( path ) != 0 ) {
	return( -1 );
    }

    return ( 0 );

error:
    i = errno;
    if ( closedir( dir ) != 0 ) {
	errno = i;
    }
    return( -1 );
}
