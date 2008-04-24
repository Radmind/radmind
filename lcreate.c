/*
 * Copyright (c) 2003 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

#include <openssl/evp.h>

#ifdef HAVE_ZLIB
#include <zlib.h>
#endif /* HAVE_ZLIB */

#include <snet.h>

#include "applefile.h"
#include "radstat.h"
#include "base64.h"
#include "cksum.h"
#include "connect.h"
#include "argcargv.h"
#include "code.h"
#include "tls.h"
#include "largefile.h"
#include "progress.h"

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
int		force = 0;
extern off_t	lsize;
extern int	showprogress;
extern char	*version;
extern char	*checksumlist;
extern struct timeval   timeout;   
const EVP_MD    *md;
SSL_CTX  	*ctx;

extern char             *caFile, *caDir, *cert, *privatekey;

    int
main( int argc, char **argv )
{
    int			c, err = 0, tac; 
    int			network = 1, len = 0, rc;
    int			negative = 0, tran_only = 0;
    int			respcount = 0;
    unsigned short	port = 0;
    extern int		optind;
    SNET          	*sn = NULL;
    char		type;
    char		*tname = NULL, *host = _RADMIND_HOST; 
    char		*p,*d_path = NULL, tline[ 2 * MAXPATHLEN ];
    char		pathdesc[ 2 * MAXPATHLEN ];
    char		**targv;
    char                cksumval[ SZ_BASE64_E( EVP_MAX_MD_SIZE ) ];
    extern char		*optarg;
    struct timeval	tv;
    FILE		*tran = NULL;
    struct stat		st;
    struct applefileinfo	afinfo;
    int                 authlevel = _RADMIND_AUTHLEVEL;
    int                 use_randfile = 0;
    int                 login = 0;
    char                *user = NULL;
    char                *password = NULL;
	char               **capa = NULL; /* capabilities */

    while (( c = getopt( argc, argv, "%c:Fh:ilnNp:P:qrt:TU:vVw:x:y:z:Z:" ))
	    != EOF ) {
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

	case 'F':
	    force = 1;
	    break;

	case 'h':
	    host = optarg; 
	    break;

	case 'i':
	    setvbuf( stdout, ( char * )NULL, _IOLBF, 0 );
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
	    /* connect.c handles things if atoi returns 0 */
            port = htons( atoi( optarg ));
	    break;

        case 'P' :              /* ca dir */
            caDir = optarg;
            break;

	case 'q':
	    quiet = 1;
	    break;

	case 'r':
	    use_randfile = 1;
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
            caFile = optarg;
            break;

        case 'y' :              /* cert file */
            cert = optarg;
            break;

        case 'z' :              /* private key */
            privatekey = optarg;
            break;

        case 'Z':
#ifdef HAVE_ZLIB
            zlib_level = atoi(optarg);
            if (( zlib_level < 0 ) || ( zlib_level > 9 )) {
                fprintf( stderr, "Invalid compression level\n" );
                exit( 1 );
            }
            break;
#else /* HAVE_ZLIB */
            fprintf( stderr, "Zlib not supported.\n" );
            exit( 1 );
#endif /* HAVE_ZLIB */

	case '?':
	    err++;
	    break;
	default:
	    err++;
	    break;
	}
    }

    if ( quiet && ( showprogress || verbose )) {
	err++;
    }
    if ( showprogress && verbose ) {
	err++;
    }

    if ( err || ( argc - optind != 1 ))   {
	fprintf( stderr, "usage: lcreate [ -%%FlnNrTV ] [ -q | -v | -i ] " );
	fprintf( stderr, "[ -c checksum ] " );
	fprintf( stderr, "[ -h host ] [ -p port ] [ -P ca-pem-directory ] " );
	fprintf( stderr, "[ -t stored-name ] [ -U user ] " );
        fprintf( stderr, "[ -w auth-level ] [ -x ca-pem-file ] " );
        fprintf( stderr, "[ -y cert-pem-file] [ -z key-pem-file ] " );
        fprintf( stderr, "[ -Z compression-level ] " );
	fprintf( stderr, "create-able-transcript\n" );
	exit( 2 );
    }

    if ( ! tran_only ) {
	if (( tran = fopen( argv[ optind ], "r" )) == NULL ) {
	    perror( argv[ optind ] );
	    exit( 2 );
	}
    }

    if ( network ) {

	/*
	 * Pipelining creates an annoying problem: the server might
	 * have closed our connection a long time before we get around
	 * to reading an error.  In the meantime, we will do a lot
	 * of writing, which may cause us to be killed.
	 */
	if ( signal( SIGPIPE, SIG_IGN ) == SIG_ERR ) {
	    perror( "signal" );
	    exit( 2 );
	}

	if ( authlevel != 0 ) {
	    if ( tls_client_setup( use_randfile, authlevel, caFile, caDir,
		    cert, privatekey ) != 0 ) {
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
	if (( capa = get_capabilities( sn )) == NULL ) { 
		exit( 2 );
	}           

        if ( authlevel != 0 ) {
            if ( tls_client_start( sn, host, authlevel ) != 0 ) {
                /* error message printed in tls_cleint_starttls */
                exit( 2 );
            }
        }

#ifdef HAVE_ZLIB
	/* Enable compression */
	if ( zlib_level > 0 ) {
	    if ( negotiate_compression( sn, capa ) != 0 ) {
		    exit( 2 );
	    }
	}
#endif /* HAVE_ZLIB */
		
        if ( login ) {
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

	    printf( "user: %s\n", user );
	    if (( password = getpass( "password:" )) == NULL ) {
		fprintf( stderr, "Invalid null password\n" );
		exit( 2 );
	    }

	    len = strlen( password );
	    if ( len == 0 ) {
		fprintf( stderr, "Invalid null password\n" );
		exit( 2 );
	    }

            if ( verbose ) printf( ">>> LOGIN %s\n", user );
            if ( snet_writef( sn, "LOGIN %s %s\n", user, password ) < 0 ) {
                fprintf( stderr, "login %s failed: 1-%s\n", user, 
                    strerror( errno ));
                exit( 2 );                       
            }                            

	    /* clear the password from memory */
	    memset( password, 0, len );

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

        }

	if ( cksum ) {
	    if ( do_cksum( argv[ optind ], cksumval ) < 0 ) {
		perror( tname );
		exit( 2 );
	    }
	}

	if ( snprintf( pathdesc, MAXPATHLEN * 2, "STOR TRANSCRIPT %s",
		tname ) >= ( MAXPATHLEN * 2 )) {
	    fprintf( stderr, "STOR TRANSCRIPT %s: path description too long\n",
		tname );
	}

	/* Get transcript size */
	if ( stat( argv[ optind ], &st ) != 0 ) {
	    perror( argv[ optind ] );
	    exit( 2 );
	}

	if ( ! tran_only ) {
	    lsize = loadsetsize( tran );
	}
	lsize += st.st_size;

	respcount += 2;
	if (( rc = stor_file( sn, pathdesc, argv[ optind ], st.st_size,
		cksumval )) <  0 ) {
	    goto stor_failed;
	}

	if ( tran_only ) {	/* don't upload files */
	    goto done;
	}
    }

    while ( fgets( tline, MAXPATHLEN, tran ) != NULL ) {
	if ( network && respcount > 0 ) {
	    tv.tv_sec = 0;
	    tv.tv_usec = 0;
	    if ( stor_response( sn, &respcount, &tv ) < 0 ) {
		exit( 2 );
	    }
	}

	len = strlen( tline );
	if (( tline[ len - 1 ] ) != '\n' ) {
	    fprintf( stderr, "%s: line too long\n", tline );
	    exit( 2 );
	}
	linenum++;
	tac = argcargv( tline, &targv );

	/* skips blank lines and comments */
	if (( tac == 0 ) || ( *targv[ 0 ] == '#' )) {
	    continue;
	}

	if ( tac == 1 ) {
	    fprintf( stderr, "Appliable transcripts cannot be uploaded.\n" );
	    exit( 2 );
	}
	if ( *targv[ 0 ] == 'f' || *targv[ 0 ] == 'a' ) {
	    if ( tac != 8 ) {
		fprintf( stderr, "line %d: invalid transcript line\n",
			linenum );
		exit( 2 );
	    }

	    if (( d_path = decode( targv[ 1 ] )) == NULL ) {
		fprintf( stderr, "line %d: path too long\n", linenum );
		return( 1 );
	    } 

	    if ( !negative ) {
		/* Verify transcript line is correct */
		if ( radstat( d_path, &st, &type, &afinfo ) != 0 ) {
		    perror( d_path );
		    exit( 2 );
		}
		if ( *targv[ 0 ] != type ) {
		    fprintf( stderr, "line %d: file type wrong\n", linenum );
		    exit( 2 );
		}
	    }

	    if ( !network ) {
		/* Check size */
		if ( radstat( d_path, &st, &type, &afinfo ) != 0 ) {
		    perror( d_path );
		    exit( 2 );
		}
		if ( st.st_size != strtoofft( targv[ 6 ], NULL, 10 )) {
		    fprintf( stderr, "line %d: size in transcript does "
			"not match size of file\n", linenum );
		    exit( 2 );
		}
		if ( cksum ) {
		    if ( *targv[ 0 ] == 'f' ) {
			if ( do_cksum( d_path, cksumval ) < 0 ) {
			    perror( d_path );
			    exit( 2 );
			}
		    } else {
			/* apple file */
			if ( do_acksum( d_path, cksumval, &afinfo ) < 0  ) {
			    perror( d_path );
			    exit( 2 );
			}
		    }
		    if ( strcmp( cksumval, targv[ 7 ] ) != 0 ) {
			fprintf( stderr,
			    "line %d: checksum listed in transcript wrong\n",
			    linenum );
			return( -1 );
		    }
		} else {
		    if ( access( d_path,  R_OK ) < 0 ) {
			perror( d_path );
			exit( 2 );
		    }
		}
	    } else {
		if ( snprintf( pathdesc, MAXPATHLEN * 2, "STOR FILE %s %s", 
			tname, targv[ 1 ] ) >= ( MAXPATHLEN * 2 )) {
		    fprintf( stderr, "STOR FILE %s %s: path description too"
			    " long\n", tname, d_path );
		    exit( 2 );
		}

		if ( negative ) {
		    if ( *targv[ 0 ] == 'a' ) {
			rc = n_stor_applefile( sn, pathdesc, d_path );
		    } else {
			rc = n_stor_file( sn, pathdesc, d_path );
		    }
		    respcount += 2;
		    if ( rc < 0 ) {
			goto stor_failed;
		    }

		} else {
		    if ( *targv[ 0 ] == 'a' ) {
			rc = stor_applefile( sn, pathdesc, d_path,
			    strtoofft( targv[ 6 ], NULL, 10 ), targv[ 7 ],
			    &afinfo );
		    } else {
			rc = stor_file( sn, pathdesc, d_path, 
			    strtoofft( targv[ 6 ], NULL, 10 ), targv[ 7 ]); 
		    }
		    respcount += 2;
		    if ( rc < 0 ) {
			goto stor_failed;
		    }
		}
	    }
	}
    }

done:
    if ( network ) {
	while ( respcount > 0 ) {
	    if ( stor_response( sn, &respcount, NULL ) < 0 ) {
		exit( 2 );
	    }
	}
	if (( closesn( sn )) != 0 ) {
	    fprintf( stderr, "cannot close sn\n" );
	    exit( 2 );
	}
#ifdef HAVE_ZLIB
	if ( verbose && zlib_level > 0 ) print_stats( sn );
#endif /* HAVE_ZLIB */
    }

    exit( 0 );

stor_failed:
    if ( dodots ) { putchar( (char)'\n' ); }
    while ( respcount > 0 ) {
	tv.tv_sec = 30;
	tv.tv_usec = 0;
	if ( stor_response( sn, &respcount, &tv ) < 0 ) {
	    exit( 2 );
	}
    }
    exit( 2 );
}
