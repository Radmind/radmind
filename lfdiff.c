/*
 * Copyright (c) 2003 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

#include <openssl/evp.h>
#include <snet.h>

#include "applefile.h"
#include "connect.h"
#include "argcargv.h"
#include "tls.h"

void output( char* string);

void			(*logger)( char * ) = NULL;
extern struct timeval	timeout;
int			verbose = 0;
int			dodots = 0;
int			linenum = 0;
int			cksum = 0;
const EVP_MD    	*md;
SSL_CTX  		*ctx;

extern char             *ca, *cert, *privatekey;

    void
output( char *string )
{
    printf( "<<< %s\n", string );
    return;
}

/*
 * exit codes:
 *      0       No differences were found.
 *	1	Differences were found.
 *      >1     	An error occurred. 
 */

    int
main( int argc, char **argv, char **envp )
{
    int			c, i, tac, port = htons( 6662 ), err = 0;
    int			special = 0, diffargc = 0;
    int			fd;
    extern int          optind; 
    extern char		*version;
    char		*host = _RADMIND_HOST;
    char		*transcript = NULL;
    char		*file = NULL;
    char		*diff = _PATH_GNU_DIFF;
    char		**diffargv;
    char		**argcargv;
    char 		pathdesc[ 2 * MAXPATHLEN ];
    char 		*path = "/tmp/lfdiff";
    char 		temppath[ MAXPATHLEN ];
    char		opt[ 3 ];
    struct servent	*se;
    SNET		*sn;
    int                 authlevel = _RADMIND_AUTHLEVEL;
    int                 use_randfile = 0;

    /* create argv to pass to diff */
    if (( diffargv = (char **)malloc( 1  * sizeof( char * ))) == NULL ) {
	perror( "malloc" );
	exit( 2 );
    }
    diffargc = 0;
    diffargv[ diffargc++ ] = diff;

    while (( c = getopt ( argc, argv, "h:p:rST:Vvw:x:y:z:bitcefnC:D:sX:" ))
	    != EOF ) {
	switch( c ) {
	case 'h':
	    host = optarg;
	    break;

	case 'p':
	    if (( port = htons ( atoi( optarg ))) == 0 ) {
		if (( se = getservbyname( optarg, "tcp" )) == NULL ) {
		    fprintf( stderr, "%s: service unkown\n", optarg );
		    exit( 2 );
		}
		port = se->s_port;
	    }
	    break;

	case 'r':
	    use_randfile = 1;
	    break;

	case 'S':
	    special = 1;
	    break;

	case 'T':
	    transcript = optarg;
	    break;

	case 'V':
	    printf( "%s\n", version );
	    exit( 0 );

	case 'v':
	    verbose = 1;
	    logger = output;
	    if ( isatty( fileno( stdout ))) {
		dodots = 1;
	    }
	    break;

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

	/* diff options */
	case 'b': case 'i': case 't':
	case 'c': case 'e': case 'f': case 'n':
	case 's':
	    if (( diffargv = (char **)realloc( diffargv, ( sizeof( *diffargv )
		    + ( 2 * sizeof( char * ))))) == NULL ) {
		perror( "malloc" );
		exit( 2 );
	    }
	    if ( snprintf( opt, sizeof( opt ), "-%c", c ) > sizeof( opt )) {
		fprintf( stderr, "-%c: too large\n", c );
		exit( 2 );
	    }
	    if (( diffargv[ diffargc++ ] = strdup( opt )) == NULL ) {
		perror( "strdup" );
		exit( 2 );
	    };
	    break;

	case 'C':

	case 'D': 
	    if (( diffargv = (char **)realloc( diffargv, ( sizeof( *diffargv )
		    + ( 3 * sizeof( char * ))))) == NULL ) {
		perror( "malloc" );
		exit( 2 );
	    }
	    if ( snprintf( opt, sizeof( opt ), "-%c", c ) > sizeof( opt )) {
		fprintf( stderr, "-%c: too large\n", c );
		exit( 2 );
	    }
	    if (( diffargv[ diffargc++ ] = strdup( opt )) == NULL ) {
		perror( "strdup" );
		exit( 2 );
	    };
	    diffargv[ diffargc++ ] = optarg;
	    break;

	case 'X':
	    if (( tac = argcargv( opt, &argcargv )) < 0 ) {
		err++;
	    }
	    if (( diffargv = (char **)realloc( diffargv, ( sizeof( *diffargv )
		    + ( tac * sizeof( char * ))))) == NULL ) {
		perror( "malloc" );
		exit( 2 );
	    }
	    for ( i = 0; i < tac; i++ ) {
		diffargv[ diffargc++ ] = argcargv[ i ];
	    }
	    break;

	case '?':
	    err++;
	    break;
	default:
	    err++;
	    break;
	}
    }

    if ((( transcript == NULL ) && ( !special ))
	    || (( special ) && ( transcript != NULL ))
	    || ( host == NULL )) {
	err++;
    }

    if ( err || ( argc - optind != 1 )) {
	fprintf( stderr, "usage: %s ", argv[ 0 ] );
	fprintf( stderr, "[ -r ] " );
	fprintf( stderr, "[ -T transcript | -S ] " );
	fprintf( stderr, "[ -h host ] [ -p port ] [ -vV ] " );
        fprintf( stderr, "[ -w authlevel ] [ -x ca-pem-file ] " );
        fprintf( stderr, "[ -y cert-pem-file] [ -z key-pem-file ] " );
	fprintf( stderr, "[ diff options ] " );
	fprintf( stderr, "[ -X \"unsupported diff options\" ] " );
	fprintf( stderr, "file\n" );
	exit( 2 );
    }
    file = argv[ optind ];

    if ( authlevel != 0 ) {
        if ( tls_client_setup( use_randfile, authlevel, ca, cert, 
                privatekey ) != 0 ) {
            /* error message printed in tls_setup */
            exit( 2 );
        }
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

    /* create path description */
    if ( special ) {
	if ( snprintf( pathdesc, ( MAXPATHLEN * 2 ), "SPECIAL %s",
		file ) >= ( MAXPATHLEN * 2 )) {
	    fprintf( stderr, "RETR SPECIAL %s: path description too long\n",
		    file );
	}
    } else {
	if ( snprintf( pathdesc, ( MAXPATHLEN * 2 ), "FILE %s %s",
		transcript, file ) >= ( MAXPATHLEN * 2 )) {
	    fprintf( stderr, "RETR FILE %s %s: path description too long\n",
		    transcript, file );
	}
    }

    if ( retr( sn, pathdesc, path, (char *)&temppath, -1, "-" ) != 0 ) {
	exit( 2 );
    }

    if (( closesn( sn )) != 0 ) {
	fprintf( stderr, "can not close sn\n" );
	exit( 2 );
    }

    if (( fd = open( temppath, O_RDONLY )) < 0 ) {
	perror( temppath );
	exit( 2 );
    } 
    if ( unlink( temppath ) != 0 ) {
	perror( temppath );
	exit( 2 );
    }
    if ( dup2( fd, 0 ) < 0 ) {
	perror( temppath );
	exit( 2 );
    }
    if (( diffargv = (char **)realloc( diffargv, ( sizeof( *diffargv )
	    + ( 4 * sizeof( char * ))))) == NULL ) {
	perror( "malloc" );
	exit( 2 );
    }
    diffargv[ diffargc++ ] = "--";
    diffargv[ diffargc++ ] = "-";
    diffargv[ diffargc++ ] = file; 
    diffargv[ diffargc++ ] = NULL;

    execve( diff, diffargv, envp );

    perror( diff );
    exit( 2 );
}
