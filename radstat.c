#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <errno.h>
#include <snet.h>
#include <stdio.h>

#include "radstat.h"
#include "applefile.h"

/* Return values:
 * < 0 system error - errno set
 *   0 okay
 *   1 error - unkown file type, errno not set
 */

    int
radstat( char *path, struct stat *st, char *type, char *finfo )
{
    char		rsrc_path[ MAXPATHLEN ];
    struct stat		rsrc_st;
#ifdef __APPLE__
    static char		null_buf[ 32 ] = { 0 };
    struct {
        unsigned long   ssize;
        char            finfo_buf[ 32 ];
    } finfo_struct;
#endif __APPLE__

    if ( lstat( path, st ) != 0 ) {
	return( -1 );
    }
    switch( (int)type ) {
    case S_IFREG:
	if ( finfo == NULL ) {
	    goto regularfile;
	} else {
#ifdef __APPLE__
	    /* Check to see if it's an HFS+ file */
	    if ( getattrlist( path, &alist, &finfo_struct,
		    sizeof( finfo_struct ), FSOPT_NOFOLLOW ) != 0 ) {
		goto regularfile;
	    }
	    if ( memcmp( finfo_struct.finfo_buf, null_buf,
		    sizeof( null_buf )) == 0 ) {
		goto regularfile;
	    } else {
		memcpy( finfo, finfo_struct.finfo_buf,
		    sizeof( finfo_struct.finfo_buf ));
		*type = 'a';
		break;
	    }
#endif __APPLE__
	}
regularfile:
	*type = 'f';
	break;
    case S_IFDIR:
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
    case S_IFDOOR:
	*type = 'D';
	break;
    case S_IFIFO:
	*type = 'p';
	break;
    case S_IFSOCK:
	*type = 's';
	break;
    default:
	return ( 1 );
    }

    /* Calculate full size of applefile */
    if ( *type == 'a' ) {
	if ( snprintf( rsrc_path, MAXPATHLEN, "%s/..namedfork/rsrc", path )
		> MAXPATHLEN -1 ) {
	    errno = ENAMETOOLONG;
	    return( -1 );
	}
	if ( lstat( rsrc_path, &rsrc_st ) != 0 ) {
	    return( -1 );
	}
	st->st_size += rsrc_st.st_size;		// Add rsrc fork size
	st->st_size += AS_HEADERLEN;		// Add header size
	st->st_size += ( 3 * sizeof( struct as_entry ));// Add entry size
	st->st_size += 32;			// Add finder info size
    }

    return( 0 );
}
