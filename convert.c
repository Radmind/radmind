#include <sys/types.h>
#include <sys/stat.h>

#include "convert.h"

    char
t_convert( int type )
{
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
#ifdef SOLARIS
    case S_IFDOOR:
	return ( 'D' );
#endif
    case S_IFIFO:
	return ( 'p' );
    case S_IFSOCK:
	return ( 's' );
    default:
	return ( 0 );
    }
}
