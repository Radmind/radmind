/*
 * Copyright (c) 2000 Regents of The University of Michigan.
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
#include <strings.h>
#include <unistd.h>

#include <snet.h>

#include "command.h"

int		debug = 0;
int		backlog = 5;
int		verbose = 0;
int		chksum = 0;
char		*path_radmind = _PATH_RADMIND;
char		*remote_host;

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
    struct hostent	*hp;
    struct servent	*se;
    int			c, s, err = 0, fd, sinlen, trueint;
    int			dontrun = 0;
    char		*prog, *p;
    unsigned short	port = 0;
    extern int		optind;
    extern char		*optarg;


    if (( prog = strrchr( av[ 0 ], '/' )) == NULL ) {
	prog = av[ 0 ];
    } else {
	prog++;
    }

    while (( c = getopt( ac, av, "Vcdp:b:u:" )) != EOF ) {
	switch ( c ) {
	case 'V' :		/* virgin */
	    printf( "%s\n", version );
	    exit( 0 );

	case 'c' :		/* check config files */
	    dontrun++;
	    break;

	case 'd' :		/* debug */
	    debug++;
	    break;

	case 'p' :		/* TCP port */
	    port = htons( atoi( optarg ));
	    break;

	case 'b' :		/* listen backlog */
	    backlog = atoi( optarg );
	    break;

	case 'u' :		/* umask */
	    umask( strtol( optarg, (char **)NULL, 0 ));
	    break;

	default :
	    err++;
	}
    }

    if ( chdir( _PATH_RADMIND ) < 0 ) {
	perror( _PATH_RADMIND );
	exit( 1 );
    }

    /* read config files */

    if ( dontrun ) {
	exit( 0 );
    }

    if ( err || optind != ac ) {
	fprintf( stderr, "Usage: radmind [ -d ] [ -c ] [ -p port ]" );
	fprintf( stderr, "[ -b backlog ] [ -u umask\n" );
	exit( 1 );
    }

    if ( port == 0 ) {
	if (( se = getservbyname( "rap", "tcp" )) == NULL ) {
	    fprintf( stderr, "%s: can't find rap service\n%s: continuing...\n",
		    prog, prog );
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
#else ultrix
    openlog( prog, LOG_NOWAIT|LOG_PID, LOG_RADMIND );
#endif ultrix

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

	    if (( hp = gethostbyaddr( (char *)&sin.sin_addr,
		    sizeof( struct in_addr ), AF_INET )) == NULL ) {
		syslog( LOG_ERR, "gethostbyaddr: %s: %d",
			inet_ntoa( sin.sin_addr ), h_errno );
		/* give an error banner */
		exit( 1 );
	    }

	    /* set global remote_host for retr command */
	    remote_host = strdup( hp->h_name );
	    for ( p = remote_host; *p != '\0'; p++ ) {
		*p = tolower( *p );
	    }
	    syslog( LOG_INFO, "child for %s %s",
		    inet_ntoa( sin.sin_addr ), hp->h_name );

	    exit( cmdloop( fd ));

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
