/*
 * Copyright (c) 2002 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include <unistd.h>

#ifdef TLS
#include <openssl/ssl.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#endif /* TLS */

#include <snet.h>

#include "command.h"
#include "logname.h"

void            (*logger)( char * ) = NULL;

int		debug = 0;
int		backlog = 5;
int		verbose = 0;
int		dodots = 0;
int		cksum = 0;
int		authlevel = 0;
char		*radmind_path = _RADMIND_PATH;
SSL_CTX         *ctx = NULL;

extern char	*version;

void		hup ___P(( int ));
void		chld ___P(( int ));
int		main ___P(( int, char *av[] ));

    void
hup( sig )
    int			sig;
{

    syslog( LOG_INFO, "reload %s", version );
    return;
}

    void
chld( sig )
    int			sig;
{
    int			pid, status;
    extern int		errno;

    while (( pid = waitpid( 0, &status, WNOHANG )) > 0 ) {
	if ( WIFEXITED( status )) {
	    if ( WEXITSTATUS( status )) {
		syslog( LOG_ERR, "child %d exited with %d", pid,
			WEXITSTATUS( status ));
	    } else {
		syslog( LOG_INFO, "child %d done", pid );
	    }
	} else if ( WIFSIGNALED( status )) {
	    syslog( LOG_ERR, "child %d died on signal %d", pid,
		    WTERMSIG( status ));
	} else {
	    syslog( LOG_ERR, "child %d died", pid );
	}
    }

    if ( pid < 0 && errno != ECHILD ) {
	syslog( LOG_ERR, "wait3: %m" );
	exit( 1 );
    }
    return;
}

    int
main( ac, av )
    int		ac;
    char	*av[];
{
    struct sigaction	sa, osahup, osachld;
    struct sockaddr_in	sin;
    struct servent	*se;
    int			c, s, err = 0, fd, sinlen, trueint;
    int			dontrun = 0;
    int			use_randfile = 0;
    int			ssl_mode = 0;
    char		*prog;
    unsigned short	port = 0;
    int			facility = _RADMIND_LOG;
    extern int		optind;
    extern char		*optarg;
    char		*ca = "ca.pem";
    char		*cert = "cert.pem";
    char		*privatekey = "cert.pem";


    if (( prog = strrchr( av[ 0 ], '/' )) == NULL ) {
	prog = av[ 0 ];
    } else {
	prog++;
    }

    while (( c = getopt( ac, av, "b:d:D:Lp:u:Vw:x:y:z:" )) != EOF ) {
	switch ( c ) {
	case 'b' :		/* listen backlog */
	    backlog = atoi( optarg );
	    break;

	case 'd' :		/* debug */
	    debug++;
	    break;

	case 'D':		/* Set radmind path */
	    radmind_path = optarg;
	    break;

	case 'L' :		/* syslog facility */
	    if (( facility = syslogname( optarg )) == -1 ) {
		fprintf( stderr, "%s: %s: unknown syslog facility\n",
			prog, optarg );
		exit( 1 );
	    }
	    break;

	case 'p' :		/* TCP port */
	    port = htons( atoi( optarg ));
	    break;

	case 'u' :		/* umask */
	    umask( (mode_t)strtol( optarg, (char **)NULL, 0 ));
	    break;

	case 'V' :		/* version */
	    printf( "%s\n", version );
	    exit( 0 );

	case 'w' :		/* authlevel 0:none, 1:serv, 2:client & serv */
	    authlevel = atoi( optarg );
	    if (( authlevel < 0 ) || ( authlevel > 2 )) {
		fprintf( stderr, "%s: %s: invalid authorization level\n",
			prog, optarg );
		exit( 1 );
	    }
	    break;

	case 'x' :		/* ca file */
	    ca = optarg;
	    break;

	case 'y' :		/* cert file */
	    cert = optarg;
	    break;

	case 'z' :		/* private key */
	    privatekey = optarg;
	    break;

	default :
	    err++;
	}
    }

    if ( err || optind != ac ) {
	fprintf( stderr, "Usage: radmind [ -b backlog ] " );
	fprintf( stderr, "[ -D path ] [ -d ] [ -L syslog-facility ] " );
	fprintf( stderr, "[ -p port ] [ -u umask ] [ -V ]\n" );
	exit( 1 );
    }

    if ( authlevel != 0 ) {
	/* Setup SSL */

        SSL_load_error_strings();
        SSL_library_init();    

        if ( use_randfile ) {
            char        randfile[ MAXPATHLEN ];      

            if ( RAND_file_name( randfile, sizeof( randfile )) == NULL ) {
                fprintf( stderr, "RAND_file_name: %s\n",
                        ERR_error_string( ERR_get_error(), NULL ));
                exit( 1 );
            }
            if ( RAND_load_file( randfile, -1 ) <= 0 ) {
                fprintf( stderr, "RAND_load_file: %s: %s\n", randfile,
                        ERR_error_string( ERR_get_error(), NULL ));
                exit( 1 );
            }
            if ( RAND_write_file( randfile ) < 0 ) {
                fprintf( stderr, "RAND_write_file: %s: %s\n", randfile,
                        ERR_error_string( ERR_get_error(), NULL ));
                exit( 1 );
            }
        }

        if (( ctx = SSL_CTX_new( SSLv23_server_method())) == NULL ) {
            fprintf( stderr, "SSL_CTX_new: %s\n",
                    ERR_error_string( ERR_get_error(), NULL ));
            exit( 1 );
        }

	if ( SSL_CTX_use_PrivateKey_file( ctx, privatekey,
		SSL_FILETYPE_PEM ) != 1 ) {
	    fprintf( stderr, "SSL_CTX_use_PrivateKey_file: %s: %s\n",
		    privatekey, ERR_error_string( ERR_get_error(), NULL ));
	    exit( 1 );
	}
	if ( SSL_CTX_use_certificate_chain_file( ctx, cert ) != 1 ) {
	    fprintf( stderr, "SSL_CTX_use_certificate_chain_file: %s: %s\n",
		    cert, ERR_error_string( ERR_get_error(), NULL ));
	    exit( 1 );
	}
	/* Verify that private key matches cert */
	if ( SSL_CTX_check_private_key( ctx ) != 1 ) {
	    fprintf( stderr, "SSL_CTX_check_private_key: %s\n",
		    ERR_error_string( ERR_get_error(), NULL ));
	    exit( 1 );
	}

	if ( authlevel == 2 ) {
        /* Load CA */
	    if ( SSL_CTX_load_verify_locations( ctx, ca, NULL ) != 1 ) {
		fprintf( stderr, "SSL_CTX_load_verify_locations: %s: %s\n",
			ca, ERR_error_string( ERR_get_error(), NULL ));
		exit( 1 );
	    }
	}
        /* Set level of security expecations */
	if ( authlevel == 1 ) {
	    ssl_mode = SSL_VERIFY_NONE; 
	} else {
	    /* authlevel == 2 */
	    ssl_mode = SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
	}
        SSL_CTX_set_verify( ctx, ssl_mode, NULL );
    }

    if ( dontrun ) {
	exit( 0 );
    }

    if ( chdir( radmind_path ) < 0 ) {
	perror( radmind_path );
	exit( 1 );
    }

    /* Create directory structure */
    if ( mkdir( "command", 0750 ) != 0 ) {
	if ( errno != EEXIST ) {
	    perror( "command" );
	    exit( 1 );
	}
    }
    if ( mkdir( "file", 0750 ) != 0 ) {
	if ( errno != EEXIST ) {
	    perror( "file" );
	    exit( 1 );
	}
    }
    if ( mkdir( "special", 0750 ) != 0 ) {
	if ( errno != EEXIST ) {
	    perror( "file" );
	    exit( 1 );
	}
    }
    if ( mkdir( "tmp", 0750 ) != 0 ) {
	if ( errno != EEXIST ) {
	    perror( "tmp" );
	    exit( 1 );
	}
    }
    if ( mkdir( "tmp/file", 0750 ) != 0 ) {
	if ( errno != EEXIST ) {
	    perror( "tmp/file" );
	    exit( 1 );
	}
    }
    if ( mkdir( "tmp/transcript", 0750 ) != 0 ) {
	if ( errno != EEXIST ) {
	    perror( "tmp/transcript" );
	    exit( 1 );
	}
    }
    if ( mkdir( "transcript", 0750 ) != 0 ) {
	if ( errno != EEXIST ) {
	    perror( "transcript" );
	    exit( 1 );
	}
    }

    if ( port == 0 ) {
	if (( se = getservbyname( "radmind", "tcp" )) == NULL ) {
	    port = htons( 6662 );
	} else {
	    port = se->s_port;
	}
    }

    /*
     * Set up listener.
     */
    if (( s = socket( PF_INET, SOCK_STREAM, 0 )) < 0 ) {
	perror( "socket" );
	exit( 1 );
    }
    memset( &sin, 0, sizeof( struct sockaddr_in ));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = port;

    trueint = 1;		/* default? */
    if ( setsockopt( s, SOL_SOCKET, SO_REUSEADDR, (void*) &trueint, 
	    sizeof(int)) < 0 ) {
	perror("setsockopt");
    }

    if ( bind( s, (struct sockaddr *)&sin, sizeof( struct sockaddr_in )) < 0 ) {
	perror( "bind" );
	exit( 1 );
    }
    if ( listen( s, backlog ) < 0 ) {
	perror( "listen" );
	exit( 1 );
    }

    /*
     * Disassociate from controlling tty.
     */
    if ( !debug ) {
	int		i, dt;

	switch ( fork()) {
	case 0 :
	    if ( setsid() < 0 ) {
		perror( "setsid" );
		exit( 1 );
	    }
	    dt = getdtablesize();
	    for ( i = 0; i < dt; i++ ) {
		if ( i != s ) {				/* keep socket open */
		    (void)close( i );
		}
	    }
	    if (( i = open( "/", O_RDONLY, 0 )) == 0 ) {
		dup2( i, 1 );
		dup2( i, 2 );
	    }
	    break;
	case -1 :
	    perror( "fork" );
	    exit( 1 );
	default :
	    exit( 0 );
	}
    }

    /*
     * Start logging.
     */
#ifdef ultrix
    openlog( prog, LOG_NOWAIT|LOG_PID );
#else /* ultrix */
    openlog( prog, LOG_NOWAIT|LOG_PID, facility );
#endif /* ultrix */

    /* catch SIGHUP */
    memset( &sa, 0, sizeof( struct sigaction ));
    sa.sa_handler = hup;
    if ( sigaction( SIGHUP, &sa, &osahup ) < 0 ) {
	syslog( LOG_ERR, "sigaction: %m" );
	exit( 1 );
    }

    /* catch SIGCHLD */
    memset( &sa, 0, sizeof( struct sigaction ));
    sa.sa_handler = chld;
    if ( sigaction( SIGCHLD, &sa, &osachld ) < 0 ) {
	syslog( LOG_ERR, "sigaction: %m" );
	exit( 1 );
    }

    syslog( LOG_INFO, "restart %s", version );

    /*
     * Begin accepting connections.
     */
    for (;;) {
	sinlen = sizeof( struct sockaddr_in );
	if (( fd = accept( s, (struct sockaddr *)&sin, &sinlen )) < 0 ) {
	    if ( errno == EINTR ) {
		continue;
	    }
	    syslog( LOG_ERR, "accept: %m" );
	    exit( 1 );
	}

	/* start child */
	switch ( c = fork()) {
	case 0 :
	    close( s );

	    /* reset CHLD and HUP */
	    if ( sigaction( SIGCHLD, &osachld, 0 ) < 0 ) {
		syslog( LOG_ERR, "sigaction: %m" );
		exit( 1 );
	    }
	    if ( sigaction( SIGHUP, &osahup, 0 ) < 0 ) {
		syslog( LOG_ERR, "sigaction: %m" );
		exit( 1 );
	    }

	    exit( cmdloop( fd, &sin ));

	case -1 :
	    close( fd );
	    syslog( LOG_ERR, "fork: %m" );
	    sleep( 10 );
	    break;

	default :
	    close( fd );
	    syslog( LOG_INFO, "child %d for %s", c, inet_ntoa( sin.sin_addr ));

	    break;
	}
    }
}
