/*
 * Copyright (c) 2003 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <errno.h>
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
#include "radstat.h"
#include "base64.h"
#include "cksum.h"
#include "connect.h"
#include "argcargv.h"
#include "code.h"
#include "tls.h"

/*
 * STOR
 * C: STOR <path-decription> "\r\n"
 * S: 350 Storing file "\r\n"
 * C: <size> "\r\n"
 * C: <size bytes of file data>
 * C: ".\r\n"
 * S: 250 File stored "\r\n"
 */

void		(*logger)( char * ) = NULL;
int		verbose = 0;
int		dodots = 0;
int		cksum = 0;
int		quiet = 0;
int		linenum = 0;
extern char	*version;
extern char	*checksumlist;
extern struct timeval   timeout;   
const EVP_MD    *md;
SSL_CTX  	*ctx;

extern char             *ca, *cert, *privatekey;

    static void
v_logger( char *line )
{
    printf( "<<< %s\n", line );
    return;
}

    int
main( int argc, char **argv )
{
    int			c, err = 0, port = htons(6662), tac; 
    int			network = 1, exitcode = 0, len, rc, lnbf = 0;
    int			negative = 0, tran_only = 0;
    extern int		optind;
    struct servent	*se;
    SNET          	*sn = NULL;
    char		type;
    char		*tname = NULL, *host = _RADMIND_HOST; 
    char		*p,*dpath = NULL, tline[ 2 * MAXPATHLEN ];
    char		pathdesc[ 2 * MAXPATHLEN ];
    char		**targv;
    char                cksumval[ SZ_BASE64_E( EVP_MAX_MD_SIZE ) ];
    extern char		*optarg;
    FILE		*tran; 
    struct stat		st;
    struct applefileinfo	afinfo;
    ssize_t		size = 0;
    int                 authlevel = _RADMIND_AUTHLEVEL;
    int                 use_randfile = 0;
    int                 login = 0;
    char                *user = NULL;
    char                *password = NULL;

    while (( c = getopt( argc, argv, "c:h:ilnNp:P:qt:TU:vVw:x:y:z:" ))
	    != EOF ) {
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

        case 'l':
            login = 1;
            break;

	case 'n':
	    network = 0;
	    break;

	case 'N':
	    negative = 1;
	    break;

	case 'p':
	    if (( port = htons( atoi( optarg ))) == 0 ) {
		if (( se = getservbyname( optarg, "tcp" )) == NULL ) {
		    fprintf( stderr, "%s: service unknown\n", optarg );
		    exit( 2 );
		}
		port = se->s_port;
	    }
	    break;

        case 'P':
            password = optarg;
            break;

	case 'q':
	    quiet = 1;
	    break;

	case 't':
	    tname = optarg;
	    break;

	case 'T':
	    tran_only = 1;
	    break;

        case 'U':
            user = optarg;
            break;

	case 'v':
	    verbose = 1;
	    logger = v_logger;
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

    if ( err || ( argc - optind != 1 ))   {
	fprintf( stderr, "usage: lcreate [ -lnNTV ] [ -q | -v | -i ] " );
	fprintf( stderr, "[ -c checksum ] " );
	fprintf( stderr, "[ -h host ] [ -p port ] [ -P password ] " );
	fprintf( stderr, "[ -t stored-name ] [ -U user ] " );
        fprintf( stderr, "[ -w authlevel ] [ -x ca-pem-file ] " );
        fprintf( stderr, "[ -y cert-pem-file] [ -z key-pem-file ] " );
	fprintf( stderr, "create-able-transcript\n" );
	exit( 2 );
    }

    if ( network ) {

	if ( authlevel != 0 ) {
	    if ( tls_client_setup( use_randfile, authlevel, ca, cert, 
		    privatekey ) != 0 ) {
		/* error message printed in tls_setup */
		exit( 2 );
	    }
	}



	/* no name given on command line, so make a "default" name */
	if ( tname == NULL ) {
	    tname = argv[ optind ];
	    /* strip leading "/"s */
	    if (( p = strrchr( tname, '/' )) != NULL ) {
		tname = ++p;
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

        if ( login ) {
	    struct timeval	tv;
	    char		*line;

	    if ( authlevel < 1 ) {
		fprintf( stderr, "login requires TLS\n" );
		exit( 2 );
	    }
            if ( user == NULL ) {
                if (( user = getlogin()) == NULL ) {
		    perror( "getlogin" );
                    exit( 2 );
                } 
            }
            if ( password == NULL ) {
		printf( "user: %s\n", user );
                if (( password = getpass( "password:" )) == NULL ) {
                    fprintf( stderr, "Invalid null password\n" );
                    exit( 2 );
                }
            }
            if ( verbose ) printf( ">>> LOGIN %s %s\n", user, password );
            if ( snet_writef( sn, "LOGIN %s %s\n", user, password ) < 0 ) {
                fprintf( stderr, "login %s failed: 1-%s\n", user, 
                    strerror( errno ));
                exit( 2 );                       
            }                            
	    tv = timeout;
	    if (( line = snet_getline_multi( sn, logger, &tv )) == NULL ) {
		fprintf( stderr, "login %s failed: 2-%s\n", user,
		    strerror( errno ));
		exit( 2 );
	    }
	    if ( *line != '2' ) {
		fprintf( stderr, "%s\n", line );
		return( 1 );
	    }

	    /* XXX At this point we should free/clear the password */
        }

	if ( cksum ) {
	    if ( do_cksum( argv[ optind ], cksumval ) < 0 ) {
		perror( tname );
		exitcode = 2;
		goto done;
	    }
	}

	if ( snprintf( pathdesc, MAXPATHLEN * 2, "STOR TRANSCRIPT %s\r\n",
		tname ) > ( MAXPATHLEN * 2 ) - 1 ) {
	    fprintf( stderr, "STOR TRANSCRIPT %s: path description too long\n",
		tname );
	}

	if (( rc = stor_file( sn, pathdesc, argv[ optind ], 0, cksumval ))
		<  0 ) {
	    exitcode = 2;
	    goto done;
	}

	if ( tran_only ) {	/* don't upload files */
	    goto done;
	}
    }

    if (( tran = fopen( argv[ optind ], "r" )) < 0 ) {
	perror( argv[ optind ] );
	exit( 2 );
    }

    while ( fgets( tline, MAXPATHLEN, tran ) != NULL ) {
	len = strlen( tline );
	if (( tline[ len - 1 ] ) != '\n' ) {
	    fprintf( stderr, "%s: line too long\n", tline );
	    exitcode = 2;
	    break;
	}
	linenum++;
	tac = argcargv( tline, &targv );

	/* skips blank lines and comments */
	if (( tac == 0 ) || ( *targv[ 0 ] == '#' )) {
	    continue;
	}

	if ( tac == 1 ) {
	    fprintf( stderr, "Appliable transcripts cannot be uploaded.\n" );
	    exitcode = 2;
	    break;
	}
	if ( *targv[ 0 ] == 'f' || *targv[ 0 ] == 'a' ) {
	    if ( tac != 8 ) {
		fprintf( stderr, "line %d: invalid transcript line\n",
			linenum );
		exitcode = 2;
		break;
	    }

	    dpath = decode( targv[ 1 ] );

	    /* Verify transcript line is correct */
	    if ( radstat( dpath, &st, &type, &afinfo ) != 0 ) {
		perror( dpath );
		exitcode = 2;
		break;
	    }
	    if ( *targv[ 0 ] != type ) {
		fprintf( stderr, "line %d: file type wrong\n", linenum );
		exitcode = 2;
		break;
	    }

	    if ( !network ) {
		if ( cksum ) {
		    if ( *targv[ 0 ] == 'f' ) {
			size = do_cksum( dpath, cksumval );
		    } else {
			/* apple file */
			size = do_acksum( dpath, cksumval, &afinfo );
		    }
		    if ( size < 0 ) {
			fprintf( stderr, "%s: %s\n", dpath, strerror( errno ));
			exitcode = 2;
			break;
		    } else if ( size != atol( targv[ 6 ] )) {
			fprintf( stderr, "line %d: size in transcript does "
			    "not match size of file\n", linenum );
			exitcode = 2;
			break;
		    }
		    if ( strcmp( cksumval, targv[ 7 ] ) != 0 ) {
			fprintf( stderr,
			    "line %d: checksum listed in transcript wrong\n",
			    linenum );
			return( -1 );
		    }
		}
		if ( access( dpath,  R_OK ) < 0 ) {
		    perror( dpath );
		    exitcode = 2;
		    break;
		}
	    } else {
		if ( snprintf( pathdesc, MAXPATHLEN * 2, "STOR FILE %s %s\r\n", 
			tname, targv[ 1 ] ) > ( MAXPATHLEN * 2 ) - 1 ) {
		    fprintf( stderr, "STOR FILE %s %s: path description too \
			long\n", tname, dpath );
		    exitcode = 2;
		    break;
		}

		if ( negative ) {
		    if (( rc = n_stor_file( sn, pathdesc,
			    decode( targv[ 1 ] ))) < 0 ) {
			fprintf( stderr, "failed to store file %s\n", dpath );
			exitcode = 2;
			break;
		    }
		} else {
		    if ( *targv[ 0 ] == 'a' ) {
			rc = stor_applefile( sn, pathdesc, decode( targv[ 1 ] ),
			    (size_t)atol( targv[ 6 ] ), targv[ 7 ], &afinfo );
		    } else {
			rc = stor_file( sn, pathdesc, decode( targv[ 1 ] ), 
			    (size_t)atol( targv[ 6 ] ), targv[ 7 ]); 
		    }
		    if ( rc < 0 ) {
			if ( dodots ) { putchar( (char)'\n' ); }
			exitcode = 2;
			goto done;
		    }
		}
	    }
	}
    }

done:
     if ( network ) {
	 if (( closesn( sn )) != 0 ) {
	     fprintf( stderr, "cannot close sn\n" );
	     exitcode = 2;
	 }
     }

    exit( exitcode );
}
