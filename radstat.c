/*
 * Copyright (c) 2002 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#ifdef __APPLE__
#include <sys/paths.h>
#include <sys/attr.h>
#endif /* __APPLE__ */
#include <sys/uio.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "applefile.h"
#include "radstat.h"

/* Return values:
 * < 0 system error - errno set
 *   0 okay
 *   1 error - unkown file type, errno not set
 */

    int
radstat( char *path, struct stat *st, char *type, struct applefileinfo *afinfo )
{
#ifdef __APPLE__
    struct stat			rsrc_st;
    static char			null_buf[ 32 ] = { 0 };
    extern struct attrlist 	alist;
#endif /* __APPLE__ */

    if ( lstat( path, st ) != 0 ) {
	return( -1 );
    }
    switch( st->st_mode & S_IFMT ) {
    case S_IFREG:
#ifdef __APPLE__
	/* Check to see if it's an HFS+ file */
	if ( afinfo != NULL ) {
	    if (( getattrlist( path, &alist, &afinfo->fi,
		    sizeof( struct finderinfo ), FSOPT_NOFOLLOW ) == 0 ) &&
    ( memcmp( afinfo->fi.fi_data, null_buf, sizeof( null_buf )) != 0 )) {
		*type = 'a';
		break;
	    }
	}
#endif /* __APPLE__ */
	*type = 'f';
	break;

    case S_IFDIR:
#ifdef __APPLE__
	/* Get any finder info */
	if ( afinfo != NULL ) {
	    getattrlist( path, &alist, &afinfo->fi,
		sizeof( struct finderinfo ), FSOPT_NOFOLLOW );
	}
#endif /* __APPLE__ */
	*type = 'd';
	break;

    case S_IFLNK:
	*type = 'l';
	break;

    case S_IFCHR:
	*type = 'c';
	break;

    case S_IFBLK:
	*type = 'b';
	break;

#ifdef sun
    case S_IFDOOR:
	*type = 'D';
	break;
#endif /* sun */

    case S_IFIFO:
	*type = 'p';
	break;

    case S_IFSOCK:
	*type = 's';
	break;

    default:
	return ( 1 );
    }

#ifdef __APPLE__
    /* Calculate full size of applefile */
    if ( *type == 'a' ) {
 
	if ( snprintf( afinfo->rsrc_path, MAXPATHLEN,
		"%s%s", path, _PATH_RSRCFORKSPEC ) > MAXPATHLEN - 1 ) {
	    errno = ENAMETOOLONG;
	    return( -1 );
	}

	/* Finder Info */
	afinfo->as_ents[AS_FIE].ae_id = ASEID_FINFO;
	afinfo->as_ents[AS_FIE].ae_offset = AS_HEADERLEN +
		( 3 * sizeof( struct as_entry ));		/* 62 */
	afinfo->as_ents[AS_FIE].ae_length = FINFOLEN;

	/* Resource Fork */
	afinfo->as_ents[AS_RFE].ae_id = ASEID_RFORK;
	afinfo->as_ents[AS_RFE].ae_offset =			/* 94 */
		( afinfo->as_ents[ AS_FIE ].ae_offset
		+ afinfo->as_ents[ AS_FIE ].ae_length );
	/* if there's no rsrc fork set rsrc fork len to zero */
	if ( lstat( afinfo->rsrc_path, &rsrc_st ) != 0 ) {
	    if ( errno == ENOENT ) {
		afinfo->as_ents[ AS_RFE ].ae_length = 0;
	    } else {
		return( -1 );
	    }
	} else {
	    afinfo->as_ents[ AS_RFE ].ae_length = ( int )rsrc_st.st_size;
	}

	/* Data Fork */
	afinfo->as_ents[AS_DFE].ae_id = ASEID_DFORK;
	afinfo->as_ents[ AS_DFE ].ae_offset =
	    ( afinfo->as_ents[ AS_RFE ].ae_offset
	    + afinfo->as_ents[ AS_RFE ].ae_length );
	afinfo->as_ents[ AS_DFE ].ae_length = (long)st->st_size;

	afinfo->as_size = afinfo->as_ents[ AS_DFE ].ae_offset
	    + afinfo->as_ents[ AS_DFE ].ae_length;

	/* Set st->st_size to size of encoded apple single file */
	st->st_size = afinfo->as_size;
    }
#endif /* __APPLE__ */

    return( 0 );
}
