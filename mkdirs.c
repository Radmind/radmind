#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <strings.h>

#include "mkdirs.h"

    int 
mkdirs( char *path ) 
{
    char 	*p;

    for ( p = path; *p == '/'; p++ )
	;
    for ( p = strchr( p, '/' ); p != NULL; p = strchr( p, '/' )) {
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
