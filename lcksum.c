#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <sha.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/mkdev.h>
#include <sys/ddi.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <strings.h>

#include "argcargv.h"
#include "chksum.h"

int		linenum = 0;
int		chksum = 1;
int		verbose = 0;

/*
 * exit codes:
 *	0 	No changes found, everything okay
 *	1	Changes necessary / changes made
 *	2	System error
 */

    int
main( int argc, char **argv )
{
    int			ufd, c, err = 0, update = 0, utime = 0, ucount = 0;
    int			len, tac, amode = R_OK;
    extern int          optind;
    char		*version = "1.0";
    char		*transcript = NULL, *tpath = NULL, *line;
    char                **targv;
    char                tline[ 2 * MAXPATHLEN ];
    char		path[ 2 * MAXPATHLEN ];
    char		upath[ 2 * MAXPATHLEN ];
    char		lchksum[ 29 ];
    FILE		*f, *ufs;
    struct stat		stats;

    while ( ( c = getopt ( argc, argv, "T:uVv" ) ) != EOF ) {
	switch( c ) {
	case 'T':
	    tpath = optarg;
	    break;
	case 'u':
	    amode = R_OK | W_OK;
	    update = 1;
	    break;
	case 'V':
	    printf( "%s\n", version );
	    exit( 0 );
	case 'v':
	    verbose = 1;
	    break;
	case '?':
	    err++;
	    break;
	default:
	    err++;
	    break;
	}
    }

    if ( tpath == NULL ) {
	err++;
    }

    if ( err || ( argc - optind != 0 ) ) {
	fprintf( stderr, "usage: lchksum [ -uvV ] " );
	fprintf( stderr, "-T transcript\n" );
	exit( 2 );
    }

    if ( access( tpath, amode ) !=0 ) {
	perror( tpath );
	exit( 2 );
    }

    if ( ( f = fopen( tpath, "r" ) ) == NULL ) {
	perror( tpath );
	exit( 2 );
    }

    if ( update ) {
	sprintf( upath, "%s.%d", tpath, (int)getpid() );

	if ( stat( tpath, &stats ) != 0 ) {
	    perror( tpath );
	    exit( 2 );
	}

	/* Open file */
	if ( ( ufd = open( upath, O_WRONLY | O_CREAT | O_EXCL,
		stats.st_mode ) ) < 0 ) {
	    perror( upath );
	    exit( 2 );
	}
	if ( ( ufs = fdopen( ufd, "w" ) ) == NULL ) {
	    perror( upath );
	    exit( 2 );
	}
    }

    /* Get transcript name from transcript path */
    if ( ( transcript = strrchr( tpath, '/' ) ) == NULL ) {
	transcript = tpath;
	tpath = ".";
    } else {
	*transcript = (char)'\0';
	transcript++;
    }

    while ( fgets( tline, MAXPATHLEN, f ) != NULL ) {
	linenum++;

	/* Check line length */
	len = strlen( tline );
	if (( tline[ len - 1 ] ) != '\n' ) {
	    fprintf( stderr, "%s: line too long\n", tline );
	    exit( 2 );
	}
	/* save transcript line -- must free */
	if ( ( line = strdup( tline ) ) == NULL ) {
	    perror( "strdup" );
	    exit( 2 );
	}

	tac = acav_parse( NULL, tline, &targv );
	if ( ( tac != 8 ) || ( *targv[ 0 ] != 'f' ) ) {
	    if ( update ) {
		fprintf( ufs, "%s", line );
	    }
	    goto done;
	}

	if ( *targv[ 1 ] == '/' ) {
	    sprintf( path, "%s/../file/%s%s", tpath, transcript, targv[ 1 ] );
	} else {
	    sprintf( path, "%s/../file/%s/%s", tpath, transcript, targv[ 1 ] );
	}

	if ( do_chksum( path, lchksum ) != 0 ) {
	    fprintf( stderr, "do_chksum failed on %s\n", path );
	    exit( 2 );
	}

	/* check chksum */
	if ( strcmp( lchksum, targv[ 7 ] ) != 0 ) {
	    if ( verbose ) printf( "*** %s: chksum failed\n", targv[ 1 ] );
	    if ( update ) {
		if ( verbose ) printf( "*** %s: chksum updated\n",
		    targv[ 1 ] ); 
		ucount++;
	    }
	    utime = 1;
	}
	targv[ 7 ] = lchksum;

	/* check size */
	if ( stat( path, &stats ) != 0 ) {
	    perror( tpath );
	    exit( 2 );
	}
	if ( stats.st_size != atoi( targv[ 6 ] ) ) {
	    if ( verbose ) printf( "*** %s: size incorrect\n",
		targv[ 1 ] );
	    if ( update ) {
		if ( verbose ) printf( "*** %s: size updated\n", targv[ 1 ] );
		ucount++;
	    }
	    utime = 1;
	}

	if ( update ) {
	    if ( utime ) {
		/* use local mtime */
		fprintf( ufs, "f %-37s %4s %5s %5s %9d %7d %s\n",
		    targv[ 1 ], targv[ 2 ], targv[ 3 ], targv[ 4 ],
		    (int)stats.st_mtime, (int)stats.st_size, targv[ 7 ] );
	    } else {
		/* use transcript mtime */
		fprintf( ufs, "f %-37s %4s %5s %5s %9s %7d %s\n",
		    targv[ 1 ], targv[ 2 ], targv[ 3 ], targv[ 4 ], targv[ 5 ],
		    (int)stats.st_size, targv[ 7 ] ); 
	    }
	}
done:
	free( line );
	utime = 0;
    }

    if ( update ) {

	if ( ucount ) {
	    /* reconstruct full transcript path */
	    *(transcript - 1) = '/';
	    if ( rename( upath, tpath ) != 0 ) {
		perror( upath );
		exit( 2 );
	    }
	    if ( verbose ) printf( "*** %s: updated\n", transcript );
	} else {
	    if ( unlink( upath ) != 0 ) {
		perror( upath );
		exit( 2 );
	    }
	    if ( verbose ) printf ( "*** %s: verified\n", transcript );
	}
    }

    if ( ucount ) {
	exit( 1 );
    } else {
	exit( 0 );
    }
}
