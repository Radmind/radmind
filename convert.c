#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <errno.h>
#include <snet.h>
#include <stdio.h>

#include "convert.h"
#include "applefile.h"

/* Return values:
 * < 0 system error - errno set
 *   0 okay
 *   1 error - unkown file type
 */

    int
getfsoinfo( char *path, struct stat *st, char *fstype, char *finfo )
{
    char	rsrc_path[ MAXPATHLEN ];
    struct stat	rsrc_st;

    if ( lstat( path, st ) != 0 ) {
	return( -1 );
    }
    if (( *fstype = t_convert( path, finfo, st->st_mode & S_IFMT )) == 0 ) {
	fprintf( stderr, "%s is of an unknown type\n", path );
    }

    /* Calculate full size of applefile */
    if ( *fstype == 'a' ) {
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

    char
t_convert( const char *path, char *finfo, mode_t type  )
{
    switch( type ) {
    case S_IFREG:
#ifdef __APPLE__
    if ( chk_for_finfo( path, finfo ) == 0 ) {
	return ( 'a' );
    }
#endif __APPLE__
	return ( 'f' );
    case S_IFDIR:
	return ( 'd' );
    case S_IFLNK:
	return ( 'l' );
    case S_IFCHR:
	return ( 'c' );
    case S_IFBLK:
	return ( 'b' );
#ifdef sun
    case S_IFDOOR:
	return ( 'D' );
#endif sun
    case S_IFIFO:
	return ( 'p' );
    case S_IFSOCK:
	return ( 's' );
    default:
	return ( 0 );
    }
}
