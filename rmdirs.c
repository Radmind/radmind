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
    int			i, len;
    char		temp[ MAXPATHLEN ];
    DIR			*dir;
    struct dirent	*dirent;
    struct stat		st;

    printf( "rmdir on: %s\n", path );

    if (( dir = opendir( path )) == NULL ) {
	return( -1 );
    }

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
                    > MAXPATHLEN ) {
                fprintf( stderr, "%s%s: path too long\n", path,
		    dirent->d_name );
		goto error;
            }           
        } else {
            if ( snprintf( temp, MAXPATHLEN, "%s/%s", path, dirent->d_name )
                    > MAXPATHLEN ) {
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
	}
	if ( unlink( temp ) != 0 ) {
	    fprintf( stderr, "%s: %s\n", temp, strerror( errno ));
	    goto error;
	}
	printf( "unlinking %s\n", temp );
    }

    if ( closedir( dir ) != 0 ) {
	return( -1 );
    }
    if ( unlink( path ) != 0 ) {
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
