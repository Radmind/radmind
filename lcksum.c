/*
 * Copyright (c) 2003 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openssl/evp.h>

#include "applefile.h"
#include "base64.h"
#include "argcargv.h"
#include "cksum.h"
#include "code.h"
#include "pathcmp.h"
#include "largefile.h"
#include "progress.h"
#include "root.h"

void            (*logger)( char * ) = NULL;

int		linenum = 0;
int		cksum = 0;
int		verbose = 1;
const EVP_MD	*md;
extern int	showprogress;
extern off_t	lsize;
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
    int			prefixfound = 0;
    int			remove = 0;
    int			exitval = 0;
    ssize_t		bytes = 0;
    extern int          optind;
    char		*tpath = NULL, *line = NULL;
    char		*prefix = NULL, *d_path = NULL;
    char                **targv;
    char		cwd[ MAXPATHLEN ];
    char		temp[ MAXPATHLEN ];
    char		file_root[ MAXPATHLEN ];
    char		tran_root[ MAXPATHLEN ];
    char		tran_name[ MAXPATHLEN ];
    char                tline[ 2 * MAXPATHLEN ];
    char		path[ 2 * MAXPATHLEN ];
    char		upath[ 2 * MAXPATHLEN ];
    char		lcksum[ SZ_BASE64_E( EVP_MAX_MD_SIZE ) ];
    FILE		*f, *ufs = NULL;
    struct stat		st;
    off_t		cksumsize;

    while ( ( c = getopt ( argc, argv, "%c:P:nqV" ) ) != EOF ) {
	switch( c ) {
	case '%':
	    showprogress = 1;
	    break;

	case 'c':
	    OpenSSL_add_all_digests();
	    md = EVP_get_digestbyname( optarg );
	    if ( !md ) {
		fprintf( stderr, "%s: unsupported checksum\n", optarg );
		exit( 2 );
	    }
	    cksum = 1;  
	    break; 

	case 'P':
	    prefix = optarg;
	    break;

	case 'n':
	    amode = R_OK;
	    updatetran = 0;
	    break;

	case 'q':
	    verbose = 0;
	    break;

	case 'V':
	    printf( "%s\n", version );
	    printf( "%s\n", checksumlist );
	    exit( 0 );

	case '?':
	    err++;
	    break;
	    
	default:
	    err++;
	    break;
	}
    }

    if ( cksum == 0 ) {
	err++;
    }

    tpath = argv[ optind ];

    if ( err || ( argc - optind != 1 ) ) {
	fprintf( stderr, "usage: %s [ -%%nqV ] ", argv[ 0 ] );
	fprintf( stderr, "[ -P prefix ] " );
	fprintf( stderr, "-c checksum transcript\n" );
	exit( 2 );
    }

    if ( getcwd( cwd, MAXPATHLEN ) == NULL ) {
	perror( "getcwd" );
	exit( 2 );
    }
    if ( *tpath == '/' ) {
	if ( strlen( tpath ) >= MAXPATHLEN ) {
	    fprintf( stderr, "%s: path too long\n", tpath );
	    exit( 2 );
	}
	strcpy( cwd, tpath );
    } else {
	if ( snprintf( temp, MAXPATHLEN, "%s/%s", cwd, tpath ) >= MAXPATHLEN ) {
	    fprintf( stderr, "%s/%s: path too long\n", cwd, tpath );
	    exit( 2 );
	}
	strcpy( cwd, temp );
    }
    if ( get_root( cwd, file_root, tran_root, tran_name ) != 0 ) {
	exit( 2 );
    }

    if ( stat( tpath, &st ) != 0 ) {
	perror( tpath );
	exit( 2 );
    }
    if ( !S_ISREG( st.st_mode )) {
	fprintf( stderr, "%s: not a regular file\n", tpath );
	return( 2 );
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

    if ( showprogress ) {
	/* calculate the loadset size */
	lsize = lcksum_loadsetsize( f, prefix );
    }

    while ( fgets( tline, MAXPATHLEN, f ) != NULL ) {
	linenum++;
	updateline = 0;

	/* Check line length */
	len = strlen( tline );
	if (( tline[ len - 1 ] ) != '\n' ) {
	    fprintf( stderr, "%s: %d: line too long\n", tpath, linenum);
	    exitval = 1;
	    goto done;
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
	if ( tac == 1 ) {
	    fprintf( stderr, "line %d: invalid transcript line\n", linenum );
	    exitval = 1;
	    goto done;
	}

	if ( *targv[ 0 ] == '-' ) {
	    remove = 1;
	    targv++;
	} else {
	    remove = 0;
	}

	if (( d_path = decode( targv[ 1 ] )) == NULL ) {
	    fprintf( stderr, "line %d: path too long\n", linenum );
	    exitval = 1;
	    goto done;
	} 
	if ( strlen( d_path ) >= MAXPATHLEN ) {
	    fprintf( stderr, "line %d: path too long\n", linenum );
	    exitval = 1;
	    goto done;
	}
	strcpy( path, d_path );

	/* check to see if file against prefix */
	if ( prefix != NULL ) {
	    if ( strncmp( d_path, prefix, strlen( prefix ))
		    != 0 ) {
		if ( updatetran ) {
		    fprintf( ufs, "%s", line );
		}
		goto done;
	    }
	    prefixfound = 1;
	}

	if ( showprogress && ( tac > 0 && *line != '#' )) {
	    progressupdate( bytes, d_path );
	}
	bytes = 0;

	if ((( *targv[ 0 ] != 'f' )  && ( *targv[ 0 ] != 'a' )) || ( remove )) {
	    if ( updatetran ) {
		fprintf( ufs, "%s", line );
	    }
	    bytes += PROGRESSUNIT;
	    goto done;
	}

	if ( tac != 8 ) {
	    fprintf( stderr, "line %d: %d arguments should be 8\n",
		    linenum, tac );
	    exitval = 1;
	    goto done;
	}

	if ( snprintf( path, MAXPATHLEN, "%s/%s/%s", file_root, tran_name,
		d_path ) > MAXPATHLEN - 1 ) {
	    fprintf( stderr, "%d: %s/%s/%s: path too long\n", linenum,
		file_root, tran_name, d_path );
	    exitval = 1;
	    goto done;
	}

	/* Check transcript order */
	if ( prepath != 0 ) {
	    if ( pathcmp( path, prepath ) < 0 ) {
		if ( updatetran ) {
		    fprintf( stderr, "line %d: bad sort order\n", linenum );
		} else {
		    fprintf( stderr,
		    "line %d: bad sort order.  Not continuing.\n", linenum );
		}
		exit( 2 );
	    }
	}
	if ( strlen( path ) >= MAXPATHLEN ) {
	    fprintf( stderr, "line %d: path too long\n", linenum );
	    exitval = 1;
	    goto done;
	}
	strcpy( prepath, path );

	/*
	 * Since this tool is run on the server, all files can be treated
	 * as regular files.
	 *
	 * HFS+ files saved onto the server are converted to applesingle files.
	 *
	 * fsdiff uses do_achskum( ) to calculate the cksum of HFS+ files.
	 *
	 * do_acksum( ) creates a cksum for the associated applesingle file.
	 */

	/* check size */
	if ( stat( path, &st) != 0 ) {
	    perror( path );
	    exit( 2 );
	}

	if (( cksumsize = do_cksum( path, lcksum )) < 0 ) {
	    perror( path );
	    exit( 2 );
	}

	/* check size */
	if ( cksumsize != strtoofft( targv[ 6 ], NULL, 10 )) {
	    if ( !updatetran ) {
		if ( verbose ) printf( "line %d: %s: size wrong\n",
		    linenum, d_path );
		exitval = 1;
	    } else {
		ucount++;
		if ( verbose ) printf( "%s: size updated\n", d_path );
	    }
	    updateline = 1;
	}
	bytes += cksumsize;
	bytes += PROGRESSUNIT;

	/* check cksum */
	if ( strcmp( lcksum, targv[ 7 ] ) != 0 ) {
	    if ( !updatetran ) {
		if ( verbose ) printf( "line %d: %s: "
		    "checksum wrong\n", linenum, d_path );
		exitval = 1;
	    } else {
		ucount++;
		if ( verbose ) printf( "%s: checksum updated\n", d_path ); 
	    }
	    updateline = 1;
	}

	if ( updatetran ) {
	    if ( updateline ) {
		/* Line incorrect */
		/* Check to see if checksum is listed in transcript */
		if ( strcmp( targv[ 7 ], "-" ) != 0) {
		    /* use mtime from server */
		    fprintf( ufs, "%s %-37s %4s %5s %5s %9ld "
			    "%7" PRIofft "d %s\n",
			targv[ 0 ], targv[ 1 ], targv[ 2 ], targv[ 3 ],
			targv[ 4 ], st.st_mtime, st.st_size, lcksum );
		} else {
		    /* use mtime from transcript */
		    fprintf( ufs, "%s %-37s %4s %5s %5s %9s "
			    "%7" PRIofft "d %s\n",
			targv[ 0 ], targv[ 1 ], targv[ 2 ], targv[ 3 ],
			targv[ 4 ], targv[ 5 ], st.st_size, lcksum );
		    }
	    } else {
		/* Line correct */
		fprintf( ufs, "%s", line );
	    }
	}
done:
	if ( updatetran && ( exitval != 0 )) {
	    exit( 2 );
	}
	free( line );
    }
    if ( showprogress ) {
	progressupdate( bytes, "" );
    }

    if ( !prefixfound && prefix != NULL ) {
	if ( verbose ) printf( "warning: prefix \"%s\" not found\n", prefix );
    }

    if ( updatetran ) {
	if ( ucount ) {
	    if ( rename( upath, tpath ) != 0 ) {
		fprintf( stderr, "rename %s to %s failed: %s\n", upath, tpath,
		    strerror( errno ));
		exit( 2 );
	    }
	    if ( verbose ) printf( "%s: updated\n", tran_name );
	    exit( 1 );
	} else {
	    if ( unlink( upath ) != 0 ) {
		perror( upath );
		exit( 2 );
	    }
	    if ( verbose ) printf( "%s: verified\n", tran_name );
	    exit( 0 );
	}
    } else {
	if ( exitval == 0 ) {
	    if ( verbose ) printf( "%s: verified\n", tran_name );
	    exit( 0 );
	} else {
	    if ( verbose ) printf( "%s: incorrect\n", tran_name );
	    exit( 1 );
	}
    }
}
