#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include "argcargv.h"
#include "largefile.h"
#include "progress.h"

int		progress = -1;
off_t		lsize = 0, total = 0;

    off_t
loadsetsize( FILE *tran )
{
    char	tline[ LINE_MAX ];
    char	**targv;
    int		tac;
    off_t	size = 0;

    while ( fgets( tline, LINE_MAX, tran ) != NULL ) {
	if (( tac = argcargv( tline, &targv )) == 0 ) {
	    continue;
	}

    	switch ( *targv[ 0 ] ) {
	case 'a':
	case 'f':
	    break;

	default:
	    continue;
	}

	size += strtoofft( targv[ 6 ], NULL, 10 );
    }

    rewind( tran );

    return( size );
}

    void
progressupdate( ssize_t bytes, char *path )
{
    int		last = progress;

    if ( bytes < 0 ) {
	return;
    }

    total += bytes;

    progress = ( int )((( float )total / ( float )lsize ) * 100 );
    if ( progress > last ) {
	printf( "%%%.2d %s\n", progress, path );
    }
}
