#include <stdio.h>
#include <errno.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "mkdirs.h"

    int 
create_directories( char *path ) 
{
    char 	*p;

    for ( p = strchr( path, '/' ); p != NULL; p = strchr( p, '/' )) {
	*p = '\0';
	if ( mkdir( path, 0777 ) < 0 ) {
	    if ( errno != EEXIST ) {
		return( -1 );
	    }
	}
	*p++ = '/';
    }

    return( 0 );
}
