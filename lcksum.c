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
#include "code.h"

int		linenum = 0;
int		chksum = 1;
int		verbose = 0;
extern char    	*version;

/*
 * exit codes:
 *	0 	No changes found, everything okay
 *	1	Changes necessary / changes made
 *	2	System error
 */

    int
main( int argc, char **argv )
{
    int			ufd, c, err = 0, updatetran = 0, updateline = 0;
    int			ucount = 0, len, tac, amode = R_OK;
    extern int          optind;
    char		*transcript = NULL, *tpath = NULL, *line;
    char                **targv;
    char                tline[ 2 * MAXPATHLEN ];
    char		path[ 2 * MAXPATHLEN ];
    char		upath[ 2 * MAXPATHLEN ];
    char		lchksum[ 29 ];
    FILE		*f, *ufs;
    struct stat		st;

    while ( ( c = getopt ( argc, argv, "T:uVv" ) ) != EOF ) {
	switch( c ) {
	case 'T':
	    tpath = optarg;
	    break;
	case 'u':
	    amode = R_OK | W_OK;
	    updatetran = 1;
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

    if ( updatetran ) {
	sprintf( upath, "%s.%d", tpath, (int)getpid() );

	if ( stat( tpath, &st ) != 0 ) {
	    perror( tpath );
	    exit( 2 );
	}

	/* Open file */
	if ( ( ufd = open( upath, O_WRONLY | O_CREAT | O_EXCL,
		st.st_mode ) ) < 0 ) {
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
	updateline = 0;

	/* Check line length */
	len = strlen( tline );
	if (( tline[ len - 1 ] ) != '\n' ) {
	    fprintf( stderr, "%s: %d: line too long\n", tpath, linenum);
	    exit( 2 );
	}
	/* save transcript line -- must free */
	if ( ( line = strdup( tline ) ) == NULL ) {
	    perror( "strdup" );
	    exit( 2 );
	}

	tac = acav_parse( NULL, tline, &targv );
	if ( ( tac != 8 ) || ( *targv[ 0 ] != 'f' ) ) {
	    if ( updatetran ) {
		fprintf( ufs, "%s", line );
	    }
	    goto done;
	}

	sprintf( path, "%s/../file/%s/%s", tpath, transcript,
		decode( targv[ 1 ] ) );

	if ( do_chksum( path, lchksum ) != 0 ) {
	    fprintf( stderr, "do_chksum failed on %s\n", path );
	    exit( 2 );
	}

	/* check chksum */
	if ( strcmp( lchksum, targv[ 7 ] ) != 0 ) {
	    if ( verbose && !updatetran ) printf( "*** %s: chksum wrong\n",
		    targv[ 1 ] );
	    if ( updatetran ) {
		if ( verbose && updatetran ) printf( "*** %s: chksum updated\n",
		    targv[ 1 ] ); 
		ucount++;
	    }
	    updateline = 1;
	}

	/* check size */
	if ( stat( path, &st) != 0 ) {
	    perror( tpath );
	    exit( 2 );
	}
	if ( st.st_size != atoi( targv[ 6 ] ) ) {
	    if ( verbose && !updatetran ) printf( "*** %s: size wrong\n",
		    targv[ 1 ] );
	    if ( updatetran ) {
		if ( verbose && updatetran ) printf( "*** %s: size updated\n",
			targv[ 1 ] );
		ucount++;
	    }
	    updateline = 1;
	}

	if ( updatetran ) {
	    if ( updateline ) {
		/* use local mtime */
		fprintf( ufs, "f %-37s %4s %5s %5s %9d %7d %s\n",
		    targv[ 1 ], targv[ 2 ], targv[ 3 ], targv[ 4 ],
		    (int)st.st_mtime, (int)st.st_size, lchksum );
	    } else {
		/* use transcript mtime */
		fprintf( ufs, "%s", line );
	    }
	}
done:
	free( line );
    }

    if ( updatetran ) {

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
