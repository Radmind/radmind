/*
 * Copyright (c) 2003 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fts.h>
#include <unistd.h>

#include "rmdirs.h"

    int
rmdirs( char *path )
{
    int		i;
    char	*pathargv[ 2 ];
    FTS		*fts;
    FTSENT	*file;

    pathargv[ 0 ] = path;
    pathargv[ 1 ] = NULL;

    if (( fts = fts_open( pathargv, FTS_PHYSICAL | FTS_NOSTAT,
	    NULL )) == NULL ) {
	return( -1 );
    }

    while (( file = fts_read( fts )) != NULL ) {
	switch( file->fts_info ) {
	case FTS_DNR:
	case FTS_ERR:
	    /* XXX - how should we report path of error? */
	    errno = file->fts_errno;
	    goto error;

	case FTS_D:				/* pre-order */
	    continue;

	case FTS_DP:				/* post-order */
	    if ( rmdir( file->fts_accpath ) != 0 ) {
		goto error;
	    }
	    break;

	default:
	    if ( unlink( file->fts_accpath ) != 0 ) {
		goto error;
	    }
	    break;
	}
	    
    }
    if ( errno ) {
	goto error;
    }
    if ( fts_close( fts ) != 0 ) {
	return( -1 );
    }
    return ( 0 );

error:
    i = errno;
    if ( fts_close( fts ) != 0 ) {
	errno = i;
    }
    return( -1 );
}
