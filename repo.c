/*
 * Copyright (c) 2006, 2007 Regents of The University of Michigan.
 * All Rights Reserved. See COPYRIGHT.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <openssl/ssl.h>
#include <openssl/rand.h>
#include <openssl/err.h>

#include <openssl/evp.h>

#ifdef HAVE_ZLIB
#include <zlib.h>
#endif /* HAVE_ZLIB */

#include <snet.h>

#include "code.h"
#include "applefile.h"
#include "connect.h"
#include "report.h"
#include "tls.h"

int			verbose = 0;
void			(*logger)( char * ) = NULL;
extern struct timeval	timeout;
extern char		*version;
extern char		*caFile, *caDir, *cert, *privatekey;
SSL_CTX			*ctx;

    int
main( int argc, char *argv[] )
{
    SNET		*sn;
    int			c, i = 0, err = 0, len;
    int			authlevel = _RADMIND_AUTHLEVEL;
    int			use_randfile = 0;
    extern int		optind;
    unsigned short	port = 0;
    char		*host = _RADMIND_HOST;
    char		*event = NULL;
    char		repodata[ MAXPATHLEN * 2 ];
    char		**capa = NULL; /* server capabilities */

    while (( c = getopt( argc, argv, "e:h:p:P:vVw:x:y:Z:z:" )) != EOF ) {
	switch ( c ) {
	case 'e':		/* event to report */
	    event = optarg;
	    break;

	case 'h':
	    host = optarg;
	    break;

	case 'p':
	    /* connect.c handles things if atoi returns 0 */
            port = htons( atoi( optarg ));
	    break;

	case 'P':
	    caDir = optarg;
	    break;

	case 'v':
	    verbose = 1;
	    logger = v_logger;
	    break;

	case 'V':
	    printf( "%s\n", version );
	    exit( 0 );

	case 'w':
	    authlevel = atoi( optarg );
	    if (( authlevel < 0 ) || ( authlevel > 2 )) {
		fprintf( stderr, "%s: invalid authorization level\n",
			optarg );
		exit( 2 );
	    }
	    break;

	case 'x':
	    caFile = optarg;
	    break;

	case 'y':
	    cert = optarg;
	    break;

	case 'z':
	    privatekey = optarg;
	    break;

	case 'Z':
#ifdef HAVE_ZLIB
            zlib_level = atoi( optarg );
            if (( zlib_level < 0 ) || ( zlib_level > 9 )) {
                fprintf( stderr, "Invalid compression level\n" );
                exit( 1 );
            }
            break;
#else /* HAVE_ZLIB */
            fprintf( stderr, "Zlib not supported.\n" );
            exit( 1 );
#endif /* HAVE_ZLIB */

	default:
	    err++;
	    break;
	}
    }

    /* repo is useless without an event defined */
    if ( event == NULL ) {
	err++;
    }

    if ( err || (( argc - optind ) < 0 )) {
	fprintf( stderr, "usage: %s -e event [ -Vv ] ", argv[ 0 ] );
	fprintf( stderr, "[ -h host ] [ -p port ] [ -P ca-pem-directory ] " );
	fprintf( stderr, "[ -w auth-level ] [ -x ca-pem-file ] " );
	fprintf( stderr, "[ -y cert-pem-file ] [ -z key-pem-file ] " );
	fprintf( stderr, "[ -Z compression-level ] [ message ... ]\n" );
	exit( 1 );
    }

    if ( argc == optind ) {	/* read message from stdin */
	if ( fgets( repodata, sizeof( repodata ), stdin ) == NULL ) {
	    perror( "fgets" );
	    exit( 2 );
	}

	len = strlen( repodata );
	if ( repodata[ len - 1 ] != '\n' ) {
	    fprintf( stderr, "report too long\n" );
	    exit( 2 );
	}
	repodata[ len - 1 ] = '\0';
    } else {
	if ( strlen( argv[ optind ] ) >= sizeof( repodata )) {
	    fprintf( stderr, "%s: too long\n", argv[ optind ] );
	    exit( 2 );
	}
	strcpy( repodata, argv[ optind ] );

	/* Skip first token in message */
	i = 1;
	for ( i += optind; i < argc; i++ ) {
	    if (( strlen( repodata ) + strlen( argv[ i ] ) + 2 )
			>= sizeof( repodata )) {
		fprintf( stderr, "%s %s: too long\n", repodata, argv[ i ] );
		exit( 2 );
	    }
	    strcat( repodata, " " );
	    strcat( repodata, argv[ i ] );
	}
    }

    if (( sn = connectsn( host, port )) == NULL ) {
	exit( 2 );
    }
    if (( capa = get_capabilities( sn )) == NULL ) {
            exit( 2 );
    }

    if ( authlevel != 0 ) {
	if ( tls_client_setup( use_randfile, authlevel, caFile, caDir, cert,
		privatekey ) != 0 ) {
	    exit( 2 );
	}
	if ( tls_client_start( sn, host, authlevel ) != 0 ) {
	    exit( 2 );
	}
    }

#ifdef HAVE_ZLIB
    /* Enable compression */
    if ( zlib_level > 0 ) {
        if ( negotiate_compression( sn, capa ) != 0 ) {
	    fprintf( stderr, "%s: server does not support reporting\n", host );
            exit( 2 );
        }
    }
#endif /* HAVE_ZLIB */

    /* Check to see if server supports reporting */
    if ( check_capability( "REPO", capa ) == 0 ) {
	fprintf( stderr, "%s: server does not support reporting\n", host );
	exit( 2 );
    }

    if ( report_event( sn, event, repodata ) != 0 ) {
	exit( 2 );
    }

    if (( closesn( sn )) != 0 ) {
	fprintf( stderr, "closesn failed.\n" );
	exit( 2 );
    }

    return( 0 );
}
