#include "config.h"

#include <sys/types.h>
#include <sys/param.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "argcargv.h"
#include "list.h"

#define K_CLIENT	0
#define K_SERVER	1

extern int		errno;

off_t			lsize( char *tpath );
off_t 			kfile_size( char *kfile, int location );

static struct list	*kfile_list;
static char		kdir[ MAXPATHLEN ];

    off_t
lsize( char *tpath )
{
    FILE		*tfp = NULL;
    unsigned long long	totalsize = 0, cursize = 0;
    char		**tav = NULL;
    char		line[ MAXPATHLEN * 2 ];
    int			tac, linenum = 1;

    if (( tfp = fopen( tpath, "r" )) == NULL ) {
        fprintf( stderr, "fopen %s: %s\n", tpath, strerror( errno ));
        exit( 2 );
    }

    while ( fgets( line, MAXPATHLEN * 2, tfp ) != NULL ) {
        linenum++;
        if (( tac = argcargv( line, &tav )) != 8 ) {
            continue;
        }

        if ( *tav[ 0 ] != 'a' && *tav[ 0 ] != 'f' ) {
            continue;
        }

	/* XXX - use strtoofft */
        cursize = strtoull( tav[ 6 ], NULL, 10 );
        if ( errno == ERANGE || errno == EINVAL ) {
            fprintf( stderr, "line %d: strtoull %s: %s\n",
                        linenum, tav[ 6 ], strerror( errno ));
            exit( 2 );
        }

        totalsize += cursize;
    }

    ( void )fclose( tfp );

    return( totalsize );
}

    off_t
kfile_size( char *kfile, int location )
{
    FILE	*fp;
    off_t	cursize = 0, total = 0;
    int		length, ac, linenum = 0;
    char	line[ MAXPATHLEN ];
    char	fullpath[ MAXPATHLEN ];
    char	*subpath;
    char	**av;

    if (( fp = fopen( kfile, "r" )) == NULL ) {
	perror( kfile );
	return( -1 );
    }

    while ( fgets( line, sizeof( line ), fp ) != NULL ) {
	linenum++;
	length = strlen( line );
	if ( line[ length - 1 ] != '\n' ) {
	    fprintf( stderr, "command: line %d: line too long\n", linenum );
	    return( -1 );
	}

	/* skips blank lines and comments */
	if ((( ac = argcargv( line, &av )) == 0 ) || ( *av[ 0 ] == '#' )) {
	    continue;
	}

	if ( ac != 2 ) {
	    fprintf( stderr, "command: line %d: expected 2 arguments, got %d\n",
		    linenum, ac );
	    return( -1 );
	} 

	switch( location ) {
	case K_CLIENT:
	    if ( snprintf( fullpath, MAXPATHLEN, "%s%s", kdir,
		    av[ 1 ] ) >= MAXPATHLEN ) {
		fprintf( stderr, "command: line %d: path too long\n",
			linenum );
		fprintf( stderr, "command: line %d: %s%s\n",
			linenum, kdir, av[ 1 ] );
		return( -1 );
	    }
	    break;

	case K_SERVER:
	    if ( *av[ 0 ] == 'k' ) {
		subpath = "command";
	    } else {
		subpath = "transcript";
	    }
	    if ( snprintf( fullpath, MAXPATHLEN, "%s/%s/%s", _RADMIND_PATH,
			subpath, av[ 1 ] ) >= MAXPATHLEN ) {
		fprintf( stderr, "command: line %d: path too long\n",
			linenum );
		fprintf( stderr, "command: line %d: %s%s\n",
			linenum, kdir, av[ 1 ] );
		return( -1 );
	    }
	    break;

	default:
	    fprintf( stderr, "unknown location\n" );
	    return( -1 );
	}

	switch( *av[ 0 ] ) {
	case 'k':				/* command file */
	    if ( list_check( kfile_list, fullpath )) {
		fprintf( stderr,
		    "%s: line %d: command file loop: %s already included\n",
		    kfile, linenum, av[ 1 ] );
		return( -1 );
	    }
	    if ( list_insert( kfile_list, fullpath ) != 0 ) {
		perror( "list_insert" );
		return( -1 );
	    }

	    if (( cursize = kfile_size( fullpath, location )) < 0 ) {
		return( -1 );
	    }
	    total += cursize;
	    break;

	case 'n':				/* negative */
	    /* XXX - include sizes from negative? */
	    continue;

	case 'p':				/* positive */
	    total += lsize( fullpath );
	    break;

	case 's':				/* special */
	    /* XXX - provide -h option to indicate client? */
	    continue;

	default:
	    fprintf( stderr, "command: line %d: '%s' invalid\n",
		    linenum, av[ 0 ] );
	    return( -1 );
	}
    }

    if ( fclose( fp ) != 0 ) {
	perror( kfile );
	return( -1 );
    }

    return( total );
}

    int
main( int ac, char *av[] )
{
    char		*path = NULL, *ext = NULL, *p;
    double		totalsize = 0;
    int			c, factor = 1024, err = 0, kfile = 0;
    extern int		optind;

    while (( c = getopt( ac, av, "bgkm" )) != EOF ) {
	switch ( c ) {
	case 'b':	/* bytes */
	    factor = 1;
	    break;

	case 'g':	/* gigabytes */
	    factor = ( 1024 * 1024 * 1024 );
	    break;

	case 'k':	/* kilobytes (default) */
	    factor = 1024;
	    break;

	case 'm':	/* megabytes */
	    factor = ( 1024 * 1024 );
	    break;

	case '?':
	    err++;
	}
    }

    path = av[ optind ];

    if ( err || ( ac - optind ) != 1 ) {
	fprintf( stderr, "Usage: %s [ -bgkmt ] { commandfile "
			"| transcript }\n", av[ 0 ] );
	exit( 1 );
    }

    if (( ext = strrchr( path, '.' )) != NULL ) {
	if ( strcmp( ++ext, "K" ) == 0 ) {
	    kfile = 1;
	    if ( strlen( path ) >= MAXPATHLEN ) {
		fprintf( stderr, "%s: path too long\n", path );
		exit( 2 );
	    }
	    strcpy( kdir, path );
	    
	    if (( p = strrchr( kdir, '/' )) == NULL ) {
		/* use working directory */
		strcpy( kdir, "./" );
	    } else {
		p++;
		*p = (char)'\0';
	    }
	    if (( kfile_list = list_new()) == NULL ) {
		perror( "list_new" );
		exit( 2 );
	    }
	    if ( list_insert( kfile_list, path ) != 0 ) {
		perror( "list_insert" );
		exit( 2 );
	    }
	}
	/* otherwise assume it's a transcript */
    }
	
    if ( kfile ) {
	if (( totalsize = ( double )kfile_size( path, K_SERVER )) < 0 ) {
	    exit( 2 );
	}
    } else {
	totalsize = ( double )lsize( path );
    }

    totalsize /= ( double )factor;

    printf( "%.2f\n", totalsize );

    return( 0 );
}
