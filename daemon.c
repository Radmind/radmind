/*
 * Copyright (c) 2003, 2007 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/resource.h>
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

#include <openssl/ssl.h>
#include <openssl/rand.h>
#include <openssl/err.h>

/*
 * for zeroconf, currently only available on Mac OS X
 */
#ifdef HAVE_DNSSD
#include <dns_sd.h>
#endif /* HAVE_DNSSD */

#ifdef HAVE_ZLIB
#include <zlib.h>
#endif /* HAVE_ZLIB */

#include <snet.h>

#include "command.h"
#include "logname.h"
#include "tls.h"

void            (*logger)( char * ) = NULL;

int		debug = 0;
int		backlog = 5;
int		verbose = 0;
int		dodots = 0;
int		cksum = 0;
int		authlevel = _RADMIND_AUTHLEVEL;
int		checkuser = 0;
int		connections = 0;
int             child_signal = 0;
int		maxconnections = _RADMIND_MAXCONNECTIONS; /* 0 = no limit */
int		rap_extensions = 1;			/* 1 for REPO */
char		*radmind_path = _RADMIND_PATH;
SSL_CTX         *ctx = NULL;

#ifdef HAVE_ZLIB
extern int 	max_zlib_level;
#endif /* HAVE_ZLIB */

extern char	*version;

void		hup( int );
void		chld( int );
int		main( int, char *av[] );

    void
hup( int sig )
{
    /* Hup does nothing at the moment */
    return;
}

    void
chld( int sig )
{
    child_signal++;
    return;

}

#ifdef HAVE_DNSSD
    static void
dnsreg_callback( DNSServiceRef dnssrv, DNSServiceFlags flags,
	DNSServiceErrorType error, const char *name, const char *regtype,
	const char *domain, void *context )
{
    if ( error == kDNSServiceErr_NoError ) {
	syslog( LOG_NOTICE, "DNSServiceRegister successful. Name: %s "
		"Type: %s Domain: %s", name, regtype, domain );
    } else {
	syslog( LOG_ERR, "DNSServiceRegister error: %d", ( int )error );
    }
}

    static DNSServiceErrorType
register_service( DNSServiceRef *dnssrv, unsigned int port,
		DNSServiceRegisterReply callback )
{
    DNSServiceErrorType	err;

    /* see dns_sd.h for API details */
    err = DNSServiceRegister( dnssrv,			/* registered service */
				0,			/* service flags */
				0,			/* interface index */
				NULL,			/* service name */
				"_radmind._tcp",	/* service type */
				NULL,			/* domain */
				NULL,			/* SRV target host */
				port,			/* port */
				0,			/* TXT len */
				NULL,			/* TXT record */
				callback,		/* callback */
				NULL );			/* context pointer */

    return( err );
}
#endif /* HAVE_DNSSD */

    int
main( int ac, char **av )
{
    struct sigaction	sa, osahup, osachld;
    struct sockaddr_in	sin;
    struct in_addr	b_addr;
    struct servent	*se;
    int			c, s, err = 0, fd, trueint;
    socklen_t		addrlen;
    int			dontrun = 0, fg = 0;
    int			use_randfile = 0;
    char		*prog;
    unsigned short	port = 0;
    int			facility = _RADMIND_LOG;
    int			level = LOG_INFO;
    extern int		optind;
    extern char		*optarg;
    extern char		*caFile, *caDir, *cert, *privatekey;
    pid_t		pid;
    int			status;
    struct rusage	usage;
#ifdef HAVE_DNSSD
    int			regservice = 0;
    DNSServiceRef	dnssrv;
    DNSServiceErrorType	dnsreg_err;
#endif /* HAVE_DNSSD */

    if (( prog = strrchr( av[ 0 ], '/' )) == NULL ) {
	prog = av[ 0 ];
    } else {
	prog++;
    }

    b_addr.s_addr = htonl( INADDR_ANY );

    /* Set appropriate TLS paths for server; default values are for client  */
    caFile = "cert/ca.pem";
    cert = "cert/cert.pem"; 	 
    privatekey = "cert/cert.pem";

    while (( c = getopt( ac, av, "a:Bb:dD:F:fL:m:p:P:Ru:UVw:x:y:z:Z:" ))
		!= EOF ) {
	switch ( c ) {
	case 'a' :		/* bind address */ 
	    if ( !inet_aton( optarg, &b_addr )) {
		fprintf( stderr, "%s: bad address\n", optarg );
		exit( 1 );
	    }
	    break;

	case 'B':		/* register as a Bonjour service */
	case 'R':		/* -R: deprecated in favor of -B */
#ifdef HAVE_DNSSD
	    regservice = 1;
	    break;
#else /* HAVE_DNSSD */
	    fprintf( stderr, "Bonjour not supported.\n" );
	    exit( 1 );
#endif /* HAVE_DNSSD */

	case 'b' :		/* listen backlog */
	    backlog = atoi( optarg );
	    break;

	case 'd' :		/* debug */
	    debug++;
	    verbose++;
	    break;

	case 'D':		/* Set radmind path */
	    radmind_path = optarg;
	    break;

	case 'F':
	    if (( facility = syslogfacility( optarg )) == -1 ) {
		fprintf( stderr, "%s: %s: unknown syslog facility\n",
			prog, optarg );
		exit( 1 );
	    }
	    break;

	case 'f':		/* run in foreground */
	    fg = 1;
	    break;

	case 'L' :		/* syslog level */
	    if (( level = sysloglevel( optarg )) == -1 ) {
		fprintf( stderr, "%s: unknown syslog level\n", optarg );
		exit( 1 );
	    }
	    break;

	case 'm':
	    maxconnections = atoi( optarg );	/* Set max connections */
	    break;

	case 'p' :		/* TCP port */
	    port = htons( atoi( optarg ));
	    break;

	case 'P' :		/* ca dir */
	    caDir = optarg;
	    break;

	case 'r' :
	    use_randfile = 1;
	    break;

	case 'u' :		/* umask */
	    umask( (mode_t)strtol( optarg, (char **)NULL, 0 ));
	    break;

	case 'U' :		/* Check User for upload */
	    checkuser = 1;
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
	    caFile = optarg;
	    break;

	case 'y' :		/* cert file */
	    cert = optarg;
	    break;

	case 'z' :		/* private key */
	    privatekey = optarg;
	    break;

	case 'Z':
#ifdef HAVE_ZLIB
	    max_zlib_level = atoi(optarg);
	    if (( max_zlib_level < 0 ) || ( max_zlib_level > 9 )) {
		fprintf( stderr, "Invalid compression level\n" );
		exit( 1 );
	    }
	    if ( max_zlib_level > 0 ) {
		rap_extensions++;
	    }
	    break;
#else /* HAVE_ZLIB */
	    fprintf( stderr, "Zlib not supported.\n" );
	    exit( 1 );
#endif /* HAVE_ZLIB */

	default :
	    err++;
	}
    }

    if ( err || optind != ac ) {
	fprintf( stderr, "Usage: radmind [ -dBrUV ] [ -a bind-address ] " );
	fprintf( stderr, "[ -b backlog ] [ -D path ] [ -F syslog-facility " );
	fprintf( stderr, "[ -L syslog-level ] [ -m max-connections ] " );
	fprintf( stderr, "[ -p port ] [ -P ca-pem-directory ] [ -u umask ] " );
	fprintf( stderr, "[ -w auth-level ] [ -x ca-pem-file ] " );
	fprintf( stderr, "[ -y cert-pem-file] [ -z key-pem-file ] " );
	fprintf( stderr, "[ -Z max-compression-level ]\n" );
	exit( 1 );
    }

    if ( maxconnections < 0 ) {
	fprintf( stderr, "%d: invalid max-connections\n", maxconnections );
	exit( 1 );
    }

    if ( checkuser && ( authlevel < 1 )) {
	fprintf( stderr, "-U requires auth-level > 0\n" );
	exit( 1 );
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
	    perror( "special" );
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

    if ( authlevel != 0 ) {
	if ( tls_server_setup( use_randfile, authlevel, caFile, caDir, cert,
		privatekey ) != 0 ) {
	    exit( 1 );
	}
    }

    if ( port == 0 ) {
	if (( se = getservbyname( "radmind", "tcp" )) == NULL ) {
	    port = htons( 6222 );
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
    sin.sin_addr.s_addr = b_addr.s_addr;
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
    if ( !debug && !fg ) {
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
    setlogmask( LOG_UPTO( level ));

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
     * Register as Bonjour service, if requested.
     * We have to wait till we've started 
     * listening for this registration to work.
     */
#ifdef HAVE_DNSSD
    if ( regservice ) {
	dnsreg_err = register_service( &dnssrv, sin.sin_port, dnsreg_callback );
	if ( dnsreg_err != kDNSServiceErr_NoError ) {
	    syslog( LOG_ERR, "Failed to register as a Bonjour service." );
	}
    }
#endif /* HAVE_DNSSD */

    /*
     * Begin accepting connections.
     */
    for (;;) {

	if ( child_signal > 0 ) {
	    double	utime, stime;

	    child_signal = 0;
	    /* check to see if any children need to be accounted for */
#ifdef HAVE_WAIT4
	    while (( pid = wait4( 0, &status, WNOHANG, &usage )) > 0 ) {
#else
            while (( pid = wait3(&status, WNOHANG, &usage )) > 0 ) {
#endif
		connections--;

		/* Print stats */
		utime = usage.ru_utime.tv_sec
		    + 1.e-6 * (double) usage.ru_utime.tv_usec;
		stime = (double) usage.ru_stime.tv_sec
		    + 1.e-6 * (double) usage.ru_stime.tv_usec;
		if ( debug ) {
		    printf( 
			"child %d User time %.3fs, System time %.3fs\n",
			pid, utime, stime );
		} 
		syslog( LOG_ERR, "child %d User time %.3fs, System time %.3fs",
		    pid, utime, stime );

		if ( WIFEXITED( status )) {
		    if ( WEXITSTATUS( status )) {
			if ( debug ) {
			    printf( "child %d exited with %d\n", pid,
				    WEXITSTATUS( status ));
			} else {
			    syslog( LOG_ERR, "child %d exited with %d", pid,
				    WEXITSTATUS( status ));
			}

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
		syslog( LOG_ERR, "waitpid: %m" );
		exit( 1 );
	    }
	}

	addrlen = sizeof( struct sockaddr_in );
	if (( fd = accept( s, (struct sockaddr *)&sin, &addrlen )) < 0 ) {
	    if ( errno != EINTR ) {
		syslog( LOG_ERR, "accept: %m" );
	    }
	    continue;
	}

	connections++;

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
    
#ifdef HAVE_DNSSD
    if ( regservice ) 
	DNSServiceRefDeallocate( dnssrv );
#endif /* HAVE_DNSSD */
}
