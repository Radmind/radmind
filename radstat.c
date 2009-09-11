/*
 * Copyright (c) 2003 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include "config.h"

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
 *   1 error - unknown file type, errno not set
 */

    int
radstat( char *path, struct radstat *rs )
{
#ifdef __APPLE__
    static char			null_buf[ FINFOLEN ] = { 0 };
    extern struct attrlist 	getalist;
    extern struct attrlist 	getdiralist;
#endif /* __APPLE__ */

    if ( lstat( path, &rs->rs_stat ) != 0 ) {
	if (( errno == ENOTDIR ) || ( errno == ENOENT )) {
	    memset( &rs->rs_stat, 0, sizeof( struct stat ));
	    rs->rs_type = 'X';
	}
	return( -1 );
    }

    switch( rs->rs_stat.st_mode & S_IFMT ) {
    case S_IFREG:
#ifdef __APPLE__
	/* Check to see if it's an HFS+ file */
	if (( getattrlist( path, &getalist, &rs->rs_afinfo.ai,
		sizeof( struct attr_info ), FSOPT_NOFOLLOW ) == 0 )) {
	    if (( rs->rs_afinfo.ai.ai_rsrc_len > 0 ) ||
    ( memcmp( rs->rs_afinfo.ai.ai_data, null_buf, FINFOLEN ) != 0 )) {
		rs->rs_type = 'a';
		break;
	    }
	}
#endif /* __APPLE__ */
	rs->rs_type = 'f';
	break;

    case S_IFDIR:
#ifdef __APPLE__
	/* Get any finder info */
	getattrlist( path, &getdiralist, &rs->rs_afinfo.ai,
	    sizeof( struct attr_info ), FSOPT_NOFOLLOW );
#endif /* __APPLE__ */
	rs->rs_type = 'd';
	break;

    case S_IFLNK:
	rs->rs_type = 'l';
	break;

    case S_IFCHR:
	rs->rs_type = 'c';
	break;

    case S_IFBLK:
	rs->rs_type = 'b';
	break;

#ifdef sun
    case S_IFDOOR:
	rs->rs_type = 'D';
	break;
#endif /* sun */

    case S_IFIFO:
	rs->rs_type = 'p';
	break;

    case S_IFSOCK:
	rs->rs_type = 's';
	break;

    default:
	return ( 1 );
    }

#ifdef __APPLE__
    /* Calculate full size of applefile */
    if ( rs->rs_type == 'a' ) {

	/* Finder Info */
	rs->rs_afinfo.as_ents[AS_FIE].ae_id = ASEID_FINFO;
	rs->rs_afinfo.as_ents[AS_FIE].ae_offset = AS_HEADERLEN +
		( 3 * sizeof( struct as_entry ));		/* 62 */
	rs->rs_afinfo.as_ents[AS_FIE].ae_length = FINFOLEN;

	/* Resource Fork */
	rs->rs_afinfo.as_ents[AS_RFE].ae_id = ASEID_RFORK;
	rs->rs_afinfo.as_ents[AS_RFE].ae_offset =		/* 94 */
		( rs->rs_afinfo.as_ents[ AS_FIE ].ae_offset
		+ rs->rs_afinfo.as_ents[ AS_FIE ].ae_length );
	rs->rs_afinfo.as_ents[ AS_RFE ].ae_length =
		rs->rs_afinfo.ai.ai_rsrc_len;

	/* Data Fork */
	rs->rs_afinfo.as_ents[AS_DFE].ae_id = ASEID_DFORK;
	rs->rs_afinfo.as_ents[ AS_DFE ].ae_offset =
	    ( rs->rs_afinfo.as_ents[ AS_RFE ].ae_offset
	    + rs->rs_afinfo.as_ents[ AS_RFE ].ae_length );
	rs->rs_afinfo.as_ents[ AS_DFE ].ae_length =
		(u_int32_t)rs->rs_stat.st_size;

	rs->rs_afinfo.as_size = rs->rs_afinfo.as_ents[ AS_DFE ].ae_offset
	    + rs->rs_afinfo.as_ents[ AS_DFE ].ae_length;

	/* Set st->st_size to size of encoded apple single file */
	rs->rs_stat.st_size = rs->rs_afinfo.as_size;
    }
#endif /* __APPLE__ */

#ifdef ENABLE_XATTR
    memset( &rs->rs_xlist, 0, sizeof( struct xattrlist ));
    switch ( rs->rs_type ) {
    case 'a': case 'f': case 'd': case 'l':
	if (( rs->rs_xlist.x_len = xattr_list( path,
					&rs->rs_xlist.x_data )) < 0 ) {
	    return( -1 );
	}

    default:
	break;
    }
    rs->rs_xname = NULL;
#endif /* ENABLE_XATTR */

    return( 0 );
}
