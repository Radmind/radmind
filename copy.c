#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "copy.h"

    int
copyover( char *spath, char *dpath )
{
    char	buff[ 8 * 1024 ];
    int		src, dest;
    size_t	len;

    if ( ( src = open( spath, O_RDONLY) ) < 0 ) {
	perror( spath );
	return( 1 );
    }
    if ( ( dest = open( dpath, O_WRONLY | O_TRUNC ) ) < 0 ) {
	perror( dpath );
	return( 1 );
    }
    while ( ( len = read( src, buff, (size_t)sizeof( buff ) ) )  > 0 ) {
	if ( write( dest, buff, len ) != len ) {
	    perror( spath );
	    return( 1 );
	}
    }
    if ( len < 0 ) {
	perror( spath );
	return( 1 );
    }
    if ( close( src ) < 0 ) {
	perror( spath );
	return( 1 );
    }
    if ( close( dest ) < 0 ) {
	perror( dpath );
	return( 1 );
    }
    return( 0 );
}
    
    int
copy( char *spath, char *dpath )
{
    char	buff[ 8 * 1024 ];
    int		src, dest;
    size_t	len;

    if ( ( src = open( spath, O_RDONLY) ) < 0 ) {
	perror( spath );
	return( 1 );
    }
    if ( ( dest = open( dpath, O_WRONLY |  O_CREAT | O_EXCL ) ) < 0 ) {
	perror( dpath );
	return( 1 );
    }
    while ( ( len = read( src, buff, (size_t)sizeof( buff ) ) )  > 0 ) {
	if ( write( dest, buff, len ) != len ) {
	    perror( spath );
	    return( 1 );
	}
    }
    if ( len < 0 ) {
	perror( spath );
	return( 1 );
    }
    if ( close( src ) < 0 ) {
	perror( spath );
	return( 1 );
    }
    if ( close( dest ) < 0 ) {
	perror( dpath );
	return( 1 );
    }
    return( 0 );
}
