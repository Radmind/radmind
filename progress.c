#include "config.h"

#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "argcargv.h"
#include "code.h"
#include "largefile.h"
#include "progress.h"

int		progress = -1;
int		showprogress = 0;
off_t		lsize = 0, total = 0;

    void
linecheck( char *line, int ac, int linenum )
{
    if ( ac < 8 ) {
	if ( line[ strlen( line ) - 1 ] == '\n' ) {
	    line[ strlen( line ) - 1 ] = '\0';
	}
	fprintf( stderr, "%s: line %d: invalid transcript line\n",
			line, linenum );
	exit( 2 );
    }
}

    off_t
loadsetsize( FILE *tran )
{
    char	tline[ LINE_MAX ], line[ LINE_MAX ];
    char	**targv;
    int		tac, linenum = 0;
    off_t	size = 0;

    while ( fgets( tline, LINE_MAX, tran ) != NULL ) {
	linenum++;
	strcpy( line, tline );
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

	linecheck( line, tac, linenum );
	size += strtoofft( targv[ 6 ], NULL, 10 );
    }

    rewind( tran );

    return( size );
}

    off_t
applyloadsetsize( FILE *tran )
{
    char	tline[ LINE_MAX ], line[ LINE_MAX ];
    char	**targv;
    int		tac, linenum = 0;
    off_t	size = 0;

    while ( fgets( tline, LINE_MAX, tran ) != NULL ) {
	linenum++;
	strcpy( line, tline );
	/* skip empty lines and transcript marker lines */
	if (( tac = argcargv( tline, &targv )) <= 1 ) {
	    continue;
	}

	switch ( *targv[ 0 ] ) {
	case '+':
	    switch ( *targv[ 1 ] ) {
	    case 'a':
	    case 'f':
		linecheck( line, tac, linenum );
		size += strtoofft( targv[ 7 ], NULL, 10 );

	    default:
		break;
	    }

	default:
	    break;
	}

	size += PROGRESSUNIT;
    }

    rewind( tran );

    return( size );
}

    off_t
lcksum_loadsetsize( FILE *tran, char *prefix )
{
    char	tline[ LINE_MAX ], line[ LINE_MAX ];
    char	*d_path = NULL;
    char	**targv;
    int		tac, linenum = 0;
    off_t	size = 0;

    while ( fgets( tline, LINE_MAX, tran ) != NULL ) {
	linenum++;
	strcpy( line, tline );
	if (( tac = argcargv( tline, &targv )) <= 1 ) {
	    continue;
	}

	if ( prefix != NULL ) {
	    if (( d_path = decode( targv[ 1 ] )) == NULL ) {
		fprintf( stderr, "%d: path too long\n", linenum );
		exit( 2 );
	    }
	    if ( strncmp( d_path, prefix, strlen( prefix )) != 0 ) {
		continue;
	    }
	}

	switch ( *targv[ 0 ] ) {
	case 'a':
	case 'f':
	    linecheck( line, tac, linenum );
	    size += strtoofft( targv[ 6 ], NULL, 10 );

	default:
	    size += PROGRESSUNIT;
	    break;
	}
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
