#include <sys/stat.h>
#include <snet.h>

#include "convert.h"
#include "afile.h"

    char
t_convert( const char *path, char *finfo, int type  )
{
    int nothfs = 0;

#ifdef __APPLE__
    if ( type == S_IFDIR ) {
	nothfs++;
    } else {
    	nothfs = chk_for_finfo( path, finfo );
    }
#else !__APPLE__
    nothfs++;
#endif __APPLE__

    if ( nothfs ) {
	switch( type ) {
	case S_IFREG:
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
    } else {
	return( 'a' );		/* file is applefile, needs encoding */
    }
}
