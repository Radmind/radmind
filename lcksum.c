#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "argcargv.h"
#include "chksum.h"
#include "code.h"
#include "pathcmp.h"

int		linenum = 0;
int		chksum = 1;
int		verbose = 0;
extern char	*version, *checksumlist;
char            prepath[ MAXPATHLEN ] = {0};

/*
 * exit codes:
 *	0 	No changes found, everything okay
 *	1	Changes necessary / changes made
 *	2	System error
 */

    int
main( int argc, char **argv )
{
    int			ufd, c, err = 0, updatetran = 1, updateline = 0;
    int			ucount = 0, len, tac, amode = R_OK | W_OK;
    int			remove = 0;
    extern int          optind;
    char		*transcript = NULL, *tpath = NULL, *line;
    char		*prefix = NULL;
    char                **targv;
    char                tline[ 2 * MAXPATHLEN ];
    char		path[ 2 * MAXPATHLEN ];
    char		upath[ 2 * MAXPATHLEN ];
    char		lchksum[ 29 ];
    FILE		*f, *ufs;
    struct stat		st;

    while ( ( c = getopt ( argc, argv, "P:nvV" ) ) != EOF ) {
	switch( c ) {
	case 'P':
	    prefix = optarg;
	    break;
	case 'n':
	    amode = R_OK;
	    updatetran = 0;
	    break;
	case 'V':
	    printf( "%s\n", version );
	    printf( "%s\n", checksumlist );
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

    tpath = argv[ optind ];

    if ( err || ( argc - optind != 1 ) ) {
	fprintf( stderr, "usage: lcksum [ -nvV ] [ -P prefix ] " );
	fprintf( stderr, "transcript\n" );
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
	memset( upath, 0, MAXPATHLEN );
	if ( snprintf( upath, MAXPATHLEN, "%s.%i", tpath, (int)getpid() )
		> MAXPATHLEN - 1) {
	    fprintf( stderr, "%s.%i: path too long\n", tpath, (int)getpid() );
	}

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

        /* Skip blank lines and comments */
        if (( tac == 0 ) || ( *targv[ 0 ] == '#' )) {
	    if ( updatetran ) {
		fprintf( ufs, "%s", line );
	    }
            goto done;
        }

	if ( *targv[ 0 ] == '-' ) {
	    remove = 1;
	    targv++;
	} else {
	    remove = 0;
	}

	if ( snprintf( path, MAXPATHLEN, "%s", decode( targv[ 1 ] ))
		> MAXPATHLEN ) {
	    fprintf( stderr, "line %d: path too long\n", linenum );
	    exit( 1 );
	}
	    
	/* Check transcript order */
	if ( prepath != 0 ) {
	    if ( pathcmp( path, prepath ) < 0 ) {
		fprintf( stderr, "line %d: bad sort order\n", linenum );
		exit( 1 );
	    }
	}
	len = strlen( targv[ 1 ] );
	if ( snprintf( prepath, MAXPATHLEN, "%s", path) > MAXPATHLEN ) {
	    fprintf( stderr, "line %d: path too long\n", linenum );
	    exit( 1 );
	}

	if (( tac != 8 )
		|| (( *targv[ 0 ] != 'f' )  && ( *targv[ 0 ] != 'a' ))
		|| ( remove )) {
	    if ( updatetran ) {
		fprintf( ufs, "%s", line );
	    }
	    goto done;
	}

	/* check to see if file against prefix */
	if ( prefix != NULL ) {
	    if ( strncmp( targv[ 1 ], prefix, strlen( prefix ) ) != 0 ) {
		if ( updatetran ) {
		    fprintf( ufs, "%s", line );
		}
		goto done;
	    }
	}

	if ( snprintf( path, MAXPATHLEN, "%s/../file/%s/%s", tpath, transcript,
		decode( targv[ 1 ] )) > MAXPATHLEN - 1 ) {
	    fprintf( stderr, "%s/../file/%s/%s: path too long\n", tpath,
		transcript, decode( targv[ 1 ] ));
	    exit( 2 );
	}

	if ( do_chksum( path, lchksum ) != 0 ) {
	    perror( path );
	    exit( 2 );
	}

	/* check chksum */
	if ( strcmp( lchksum, targv[ 7 ] ) != 0 ) {
	    if ( verbose && !updatetran ) printf( "%s: chksum wrong\n",
		    decode( targv[ 1 ] ));
	    ucount++;
	    if ( updatetran ) {
		if ( verbose && updatetran ) printf( "%s: chksum updated\n",
		    decode( targv[ 1 ] )); 
	    }
	    updateline = 1;
	}

	/* check size */
	if ( stat( path, &st) != 0 ) {
	    perror( tpath );
	    exit( 2 );
	}
	if ( st.st_size != atoi( targv[ 6 ] ) ) {
	    if ( verbose && !updatetran ) printf( "%s: size wrong\n",
		    decode( targv[ 1 ] ));
	    ucount++;
	    if ( updatetran ) {
		if ( verbose && updatetran ) printf( "%s: size updated\n",
			decode( targv[ 1 ] ));
	    }
	    updateline = 1;
	}

	if ( updatetran ) {
	    if ( updateline ) {
		/* use local mtime */
		fprintf( ufs, "%s %-37s %4s %5s %5s %9d %7d %s\n",
		    targv[ 0 ], targv[ 1 ], targv[ 2 ], targv[ 3 ], targv[ 4 ],
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
	    if ( *tpath != '.' ) {
		*(transcript - 1) = '/';
	    } else {
		tpath = transcript;
	    }

	    if ( rename( upath, tpath ) != 0 ) {
		fprintf( stderr, "rename: %s %s\n", upath, tpath );
		exit( 2 );
	    }
	    if ( verbose ) printf( "%s: updated\n", transcript );
	} else {
	    if ( unlink( upath ) != 0 ) {
		perror( upath );
		exit( 2 );
	    }
	    if ( verbose ) printf( "%s: verified\n", transcript );
	}
    } else {
	if ( ucount ) {
	    if ( verbose ) printf( "%s: incorrect\n", transcript );
	    exit( 1 );
	} else {
	    if ( verbose ) printf( "%s: verified\n", transcript );
	    exit( 0 );
	}
    }
    exit( 2 );
}
