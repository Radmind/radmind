#include <sys/types.h>
#include <sys/stat.h>
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
    if ( lstat( path, st ) != 0 ) {
	return( -1 );
    }
    if (( *fstype = t_convert( path, finfo, st->st_mode & S_IFMT )) == 0 ) {
	fprintf( stderr, "%s is of an unknown type\n", path );
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
