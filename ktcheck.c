/*
 * Copyright (c) 2003 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>

#include <openssl/ssl.h>
#include <openssl/rand.h>
#include <openssl/err.h>

#include <openssl/evp.h>
#include <snet.h>

#include "applefile.h"
#include "base64.h"
#include "cksum.h"
#include "connect.h"
#include "argcargv.h"
#include "list.h"
#include "tls.h"
#include "largefile.h"

void output( char* string);
int check( SNET *sn, char *type, char *path); 
int createspecial( SNET *sn, struct node *head );
int getstat( SNET *sn, char *description, char *stats );

void			(*logger)( char * ) = NULL;
int			linenum = 0;
int			cksum = 0;
int			verbose = 0;
int			dodots= 0;
int			quiet = 0;
int			update = 1;
char			*kfile= _RADMIND_COMMANDFILE;
char			*kdir= "";
const EVP_MD		*md;
SSL_CTX  		*ctx;

extern struct timeval	timeout;
extern char		*version, *checksumlist;
extern char             *ca, *cert, *privatekey; 

    int 
getstat( SNET *sn, char *description, char *stats ) 
{
    struct timeval      tv;
    char		*line;

    if( snet_writef( sn, "STAT %s\n", description ) < 0 ) {
	perror( "snet_writef" );
	return( -1 );
    }

    if ( verbose ) printf( ">>> STAT %s\n", description );

    tv = timeout;
    if (( line = snet_getline_multi( sn, logger, &tv )) == NULL ) {
	perror( "snet_getline_multi" );
	return( -1 );
    }
    if ( *line != '2' ) {
	fprintf( stderr, "%s\n",  line );
	return( -1 );
    }

    tv = timeout;
    if (( line = snet_getline( sn, &tv )) == NULL ) {
	perror( "snet_getline 1" );
	return( -1 );
    }
    if ( snprintf( stats, MAXPATHLEN, "%s", line ) > MAXPATHLEN - 1 ) {
	fprintf( stderr, "%s: line too long\n", line );
	return( -1 );
    }

    if ( verbose ) printf( "<<< %s\n", stats );

    return( 0 );
}

    void
output( char *string )
{
    printf( "<<< %s\n", string );
    return;
}

    int
createspecial( SNET *sn, struct node *head )
{
    FILE		*fs;
    struct node 	*prev;
    char		filedesc[ MAXPATHLEN * 2 ];
    char		path[ MAXPATHLEN ];
    char		stats[ MAXPATHLEN ];

    /* Open file */
    if ( snprintf( path, MAXPATHLEN, "%sspecial.T.%i", kdir,
	    getpid()) > MAXPATHLEN - 1 ) {
	fprintf( stderr, "path too long: %sspecial.T.%i\n", kdir,
		(int)getpid());
	exit( 2 );
    }

    if (( fs = fopen( path, "w" )) == NULL ) {
	perror( path );
	return( 1 );
    }

    do {
	if ( snprintf( filedesc, MAXPATHLEN * 2, "SPECIAL %s", head->path)
		> ( MAXPATHLEN * 2 ) -1 ) {
	    fprintf( stderr, "SPECIAL %s: too long\n", head->path );
	    return( 1 );
	}

	if ( getstat( sn, (char *)&filedesc, stats ) != 0 ) {
	    return( 1 );
	}

	if ( fputs( stats, fs) == EOF ) {
	    fprintf( stderr, "fputs" );
	    return( 1 );
	}
	if ( fputs( "\n", fs) == EOF ) {
	    fprintf( stderr, "fputs" );
	    return( 1 );
	}

	prev = head;
	head = head->next;

	free( prev->path );
	free( prev );
    } while ( head != NULL );

    if ( fclose( fs ) != 0 ) {
	perror( path );
	return( 1 );
    }

    return( 0 );
}

/*
 * return codes:
 *	0	okay
 *	1	update made
 *	2	system error
 */

    int
check( SNET *sn, char *type, char *file )
{
    int		needupdate = 0;
    char	**targv;
    char	stats[ MAXPATHLEN ];
    char 	pathdesc[ 2 * MAXPATHLEN ];
    char 	tempfile[ 2 * MAXPATHLEN ];
    char        ccksum[ SZ_BASE64_E( EVP_MAX_MD_SIZE ) ];
    char	path[ MAXPATHLEN ];
    int		tac;
    struct stat		st;
    struct utimbuf      times;

    if ( file != NULL ) {
	if ( snprintf( pathdesc, MAXPATHLEN * 2, "%s %s", type, file  )
		> ( MAXPATHLEN * 2 ) - 1 ) {
	    fprintf( stderr, "%s %s: too long", type, file );
	    return( 2 );
	}

	/* create full path */
	if ( snprintf( path, MAXPATHLEN, "%s%s", kdir, file )
		> MAXPATHLEN - 1 ) {
	    fprintf( stderr, "%s%s: path too long\n", kdir, file );
	    return( 2 );
	}
    } else {
	if ( snprintf( pathdesc, MAXPATHLEN, "%s", type )
		> ( MAXPATHLEN * 2 ) - 1 ) {
	    fprintf( stderr, "%s: too long\n", type );
	    return( 2 );
	}
	file = kfile;

	/* create full path */
	if ( snprintf( path, MAXPATHLEN, "%s", kfile ) > MAXPATHLEN - 1 ) {
	    fprintf( stderr, "%s: path too long\n", kfile );
	}
    }

    if ( getstat( sn, (char *)&pathdesc, stats ) != 0 ) {
	return( 2 );
    }
    tac = acav_parse( NULL, stats, &targv );
    if ( tac != 8 ) {
	perror( "Incorrect number of arguments\n" );
	return( 2 );
    }
    times.modtime = atoi( targv[ 5 ] );
    times.actime = time( NULL );

    if (( stat( path, &st )) != 0 ) {
	if ( errno != ENOENT ) {
	    perror( path );
	    return( 2 );
	} else {
	    /* Local file is missing */
	    if ( update ) {
		if ( retr( sn, pathdesc, path, (char *)&tempfile,
			strtoofft( targv[ 6 ], NULL, 10 ), targv[ 7 ] ) != 0 ) {
		    return( 2 );
		}
		if ( utime( tempfile, &times ) != 0 ) {
		    perror( path );
		    return( 1 );
		}
		if ( rename( tempfile, path ) != 0 ) {
		    perror( tempfile );
		    return( 2 );
		}
		if ( !quiet ) printf( "%s: updated\n", path );
	    } else {
		if ( !quiet ) printf ( "%s: missing\n", path );
	    }
	    return( 1 );
	}
    }

    /*
     * With cksum we only use cksum and size.
     * Without cksum we only use mtime and size.
     */
    if ( strtoofft( targv[ 6 ], NULL, 10 ) != st.st_size ) {
	needupdate = 1;
    } else {
	if ( cksum ) {
	    if (( do_cksum( path, ccksum )) < 0 ) {
		perror( path );
		return( 2 );
	    }
	    if ( strcmp( targv[ 7 ], ccksum ) != 0 ) {
		needupdate = 1;
	    }
	} else {
	    if ( atoi( targv[ 5 ] ) != (int)st.st_mtime )  {
		needupdate = 1;
	    }
	}
    }
    if ( needupdate ) {
	if ( update ) {
	    if ( unlink( path ) != 0 ) {
		perror( path );
		return( 2 );
	    }
	    if ( retr( sn, pathdesc, path, (char *)&tempfile,
		    strtoofft( targv[ 6 ], NULL, 10 ), targv[ 7 ] ) != 0 ) {
		return( 2 );
	    }
	    if ( utime( tempfile, &times ) != 0 ) {
		perror( path );
		return( 1 );
	    }
	    if ( rename( tempfile, path ) != 0 ) {
		perror( path );
		return( 2 );
	    }
	    if ( !quiet ) printf( "%s: updated\n", path );
	} else {
	    if ( !quiet ) printf( "%s: out of date\n", path );
	}
	return( 1 );
    } else {
	return( 0 );
    }
}

/*
 * exit codes:
 *      0       No changes found, everything okay
 *      1       Changes necessary / changes made
 *      2       System error
 */

    int
main( int argc, char **argv )
{
    int			c, port = htons( 6662 ), err = 0;
    int			len, tac, change = 0, lnbf = 0;
    int			authlevel = _RADMIND_AUTHLEVEL;
    int			use_randfile = 0;
    extern int          optind;
    char		*host = _RADMIND_HOST, *p;
    char                **targv;
    char                cline[ 2 * MAXPATHLEN ];
    char		tempfile[ MAXPATHLEN ];
    char		path[ MAXPATHLEN ];
    char		lcksum[ SZ_BASE64_E( EVP_MAX_MD_SIZE ) ];
    char		tcksum[ SZ_BASE64_E( EVP_MAX_MD_SIZE ) ];
    struct servent	*se;
    SNET		*sn;
    FILE		*f;
    struct node		*head = NULL;
    struct stat		tst, lst;

    while (( c = getopt ( argc, argv, "c:h:iK:np:qrvVw:x:y:z:" )) != EOF ) {
	switch( c ) {
	case 'c':
            OpenSSL_add_all_digests();
            md = EVP_get_digestbyname( optarg );
            if ( !md ) {
                fprintf( stderr, "%s: unsupported checksum\n", optarg );
                exit( 2 );
            }
            cksum = 1;
            break;

	case 'h':
	    host = optarg;
	    break;

	case 'i':
	    setvbuf( stdout, ( char * )NULL, _IOLBF, 0 );
	    lnbf = 1;
	    break;

	case 'K':
	    kfile = optarg;
	    break;

	case 'n':
	    update = 0;
	    break;

	case 'p':
	    if (( port = htons ( atoi( optarg )) ) == 0 ) {
		if (( se = getservbyname( optarg, "tcp" )) == NULL ) {
		    fprintf( stderr, "%s: service unkown\n", optarg );
		    exit( 2 );
		}
		port = se->s_port;
	    }
	    break;

	case 'q':
	    quiet = 1;
	    break;

	case 'r':
	    use_randfile = 1;
	    break;

	case 'v':
	    verbose = 1;
	    logger = output;
	    if ( isatty( fileno( stdout ))) {
		dodots = 1;
	    }
	    break;

	case 'V':
	    printf( "%s\n", version );
	    printf( "%s\n", checksumlist );
	    exit( 0 );

        case 'w' :              /* authlevel 0:none, 1:serv, 2:client & serv */
            authlevel = atoi( optarg );
            if (( authlevel < 0 ) || ( authlevel > 2 )) {
                fprintf( stderr, "%s: invalid authorization level\n",
                        optarg );
                exit( 1 );
            }    
            break;

        case 'x' :              /* ca file */
            ca = optarg;
            break;

        case 'y' :              /* cert file */
            cert = optarg;
            break;

        case 'z' :              /* private key */
            privatekey = optarg;
            break;

	case '?':
	    err++;
	    break;
	default:
	    err++;
	    break;
	}
    }

    if ( verbose && quiet ) {
	err++;
    }
    if ( verbose && lnbf ) {
	err++;
    }

    if ( err || ( argc - optind != 0 )) {
	fprintf( stderr,
		"usage: ktcheck -c checksum [ -nrV ] [ -q | -v | -i ] " );
	fprintf( stderr, "[ -K command file ] " );
	fprintf( stderr, "[ -h host ] [ -p port ] " );
	fprintf( stderr, "[ -w authlevel ] [ -x ca-pem-file ] " );
	fprintf( stderr, "[ -y cert-pem-file] [ -z key-pem-file ]\n" );
	exit( 2 );
    }

    if ( authlevel != 0 ) {
	if ( tls_client_setup( use_randfile, authlevel, ca, cert,
		privatekey ) != 0 ) {
	    /* error message printed in tls_setup */
	    exit( 2 );
	}
    }

    if (( kdir = strdup( kfile )) == NULL ) {
        perror( "strdup failed" );
        exit( 2 );
    }
    if (( p = strrchr( kdir, '/' )) == NULL ) {
        /* No '/' in kfile - use working directory */
        kdir = "./";
    } else {
        p++;
        *p = (char)'\0';
    }
    if ( snprintf( path, MAXPATHLEN, "%s", kfile ) > MAXPATHLEN - 1 ) {
	fprintf( stderr, "%s: path too long\n", kfile );
	exit( 2 );
    }

    if (( sn = connectsn( host, port )) == NULL ) {
	exit( 2 );
    }

    if ( authlevel != 0 ) {
	if ( tls_client_start( sn, host, authlevel ) != 0 ) {
	    /* error message printed in tls_cleint_starttls */
	    exit( 2 );
	}
    }

    switch( check( sn, "COMMAND", NULL )) { 
    case 0:
	break;
    case 1:
	change++;
	if ( !update ) {
	    goto done;
	}
	break;
    case 2:
	exit( 2 );
    }

    if (( f = fopen( kfile, "r" )) == NULL ) {
	perror( kfile );
	exit( 2 );
    }

    while ( fgets( cline, MAXPATHLEN, f ) != NULL ) {
	linenum++;

	len = strlen( cline );
	if (( cline[ len - 1 ] ) != '\n' ) {
	    fprintf( stderr, "%s: %d: line too long\n", kfile, linenum );
	    exit( 2 );
	}

	tac = acav_parse( NULL, cline, &targv );

	if (( tac == 0 ) || ( *targv[ 0 ] == '#' )) {
	    continue;
	}

	if ( tac != 2 ) {
	    fprintf( stderr, "%s: %d: invalid command line\n",
		    kfile, linenum );
	    exit( 2 );
	}

	if ( *targv[ 0 ] == 's' ) {
	    insert_node( targv[ 1 ], &head);
	    continue;
	}
	    
	switch( check( sn, "TRANSCRIPT", targv[ tac - 1] )) {
	case 0:
	    break;
	case 1:
	    change++;
	    if ( !update ) {
		goto done;
	    }
	    break;
	case 2:
	    exit( 2 );
	}
    }

    if ( head != NULL ) {
	if ( createspecial( sn, head ) != 0 ) {
	    exit( 2 );
	}

	if ( snprintf( path, MAXPATHLEN, "%sspecial.T", kdir ) >
		MAXPATHLEN - 1 ) {
	    fprintf( stderr, "path too long: %sspecial.T\n", kdir );
	    exit( 2 );
	}
	if ( snprintf( tempfile, MAXPATHLEN, "%sspecial.T.%i", kdir,
		getpid()) > MAXPATHLEN - 1 ) {
	    fprintf( stderr, "path too long: %sspecial.T.%i\n", kdir,
		    (int)getpid());
	    exit( 2 );
	}
	/* get file sizes */
	if ( stat( path, &lst ) != 0 ) {
	    if ( errno == ENOENT ) {
		/* special.T did not exist */
		if ( update ) { 
		    if ( rename( tempfile, path ) != 0 ) {
			fprintf( stderr, "rename failed: %s %s\n", tempfile,
			    path );
			exit( 2 );
		    }
		    if ( !quiet ) printf( "%s: created\n", path ); 
		    change++;
		} else {
		    /* special.T not updated */
		    if ( unlink( tempfile ) !=0 ) {
			perror( tempfile );
			exit( 2 );
		    }
		}
		goto done;
	    }
	    perror( path );
	    exit( 2 );
	}
	if ( stat( tempfile, &tst ) != 0 ) {
	    perror( tempfile );
	    exit( 2 );
	}
	/* get checksums */
	if ( cksum ) {
	    if ( do_cksum( path, lcksum ) < 0 ) {
		perror( path );
		exit( 2 );
	    }
	    if ( do_cksum( tempfile, tcksum ) < 0 ) {
		perror( tempfile );
		exit( 2 );
	    }
	}

	/*
	 * Without checksums we must assume that the special 
	 * transcript has changed since there is no way to
	 * verify its contents
	 */
	/* Special exists */
	if ( !cksum ||
		(( tst.st_size != lst.st_size ) ||
		( strcmp( tcksum, lcksum) != 0 ))) {
	    change++;

	    if ( update ) {
		if ( rename( tempfile, path ) != 0 ) {
		    fprintf( stderr, "rename failed: %s %s\n", tempfile,
			    path );
		    exit( 2 );
		}
		if ( !quiet ) printf( "%s: updated\n", path ); 
	    } else {
		if ( unlink( tempfile ) !=0 ) {
		    perror( tempfile );
		    exit( 2 );
		}
	    }
	} else {
	    /* special.T not updated */
	    if ( unlink( tempfile ) !=0 ) {
		perror( tempfile );
		exit( 2 );
	    }
	}
    }

done:
    if (( closesn( sn )) !=0 ) {
	fprintf( stderr, "can not close sn\n" );
	exit( 2 );
    }

    if ( change ) {
	exit( 1 );
    } else {
	if ( !quiet ) printf( "No updates needed\n" );
	exit( 0 );
    }
}
