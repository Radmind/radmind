/*
 * Copyright (c) 2007 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/sha.h>

#ifdef HAVE_ZLIB
#include <zlib.h>
#endif /* HAVE_ZLIB */

#include <snet.h>

#include "applefile.h"
#include "cksum.h"
#include "connect.h"
#include "argcargv.h"

#define RADMIND_IANA_PORT	6222
#define RADMIND_LEGACY_PORT	6662

extern void            (*logger)( char * );
extern int              verbose;
struct timeval          timeout = { 60, 0 };
extern int		errno;
extern SSL_CTX  	*ctx;


#ifdef HAVE_ZLIB
int zlib_level = 0;
#endif

    void
v_logger( char *line )
{
    printf( "<<< %s\n", line );
    return;
}

    static SNET *
connectsn2( struct sockaddr_in *sin )
{
    int			s;
    int			one = 1;
    int			connectsn2_errno = 0;
    SNET                *sn = NULL; 
    struct protoent	*proto;

    if (( s = socket( PF_INET, SOCK_STREAM, 0 )) < 0 ) {
	perror( "socket" );
	exit( 2 );
    }
    
    if (( proto = getprotobyname( "tcp" )) == NULL ) {
	perror( "getprotobyname" );
	exit( 2 );
    }
    if ( setsockopt( s, proto->p_proto, TCP_NODELAY, &one,
	    sizeof( one )) != 0 ) {
	perror( "snet_setopt" );
	exit( 2 );
    }

    if ( verbose ) printf( "trying %s:%u... ", inet_ntoa( sin->sin_addr ),
				ntohs( sin->sin_port ));
    if ( connect( s, (struct sockaddr *)sin,
	    sizeof( struct sockaddr_in )) != 0 ) {
	connectsn2_errno = errno;
	if ( verbose ) printf( "failed: %s\n", strerror( errno ));
	(void)close( s );
	errno = connectsn2_errno;
	return( NULL );
    }
    if ( verbose ) printf( "success!\n" );
    if (( sn = snet_attach( s, 1024 * 1024 )) == NULL ) {
	perror( "snet_attach" );
	exit( 2 );
    }

    return( sn );
}

    SNET *
connectsn( char *host, unsigned short port )
{
    int			i;
    struct hostent      *he;
    struct sockaddr_in  sin;
    struct servent	*se;
    SNET                *sn = NULL; 

    memset( &sin, 0, sizeof( struct sockaddr_in ));
    sin.sin_family = AF_INET;
    if (( sin.sin_port = port ) == 0 ) {
	/* maybe radmind's in /etc/services. if not, use default. */
	if (( se = getservbyname( "radmind", "tcp" )) != NULL ) {
	    /* Port numbers are returned in network byte order */
	    sin.sin_port = se->s_port;
	} else {
	    sin.sin_port = htons( RADMIND_IANA_PORT );
	}
    }

#ifdef notdef
    /*
     * this code should be enabled only to deal with bugs in
     * the gethostbyname() routine
     */
    if (( sin.sin_addr.s_addr = inet_addr( host )) != -1 ) {
	return( connectsn2( &sin ));
    }
#endif // notdef

    if (( he = gethostbyname( host )) == NULL ) {
	fprintf( stderr, "%s: Unknown host\n", host );
	return( NULL );
    }
    
    for ( i = 0; he->h_addr_list[ i ] != NULL; i++ ) {
	memcpy( &sin.sin_addr.s_addr, he->h_addr_list[ i ],
		(unsigned int)he->h_length );

	/*
	 * radmind's original port was 6662, but got
	 * registered as 6222 with IANA, and will show
	 * up in future /etc/services as 6222. during
	 * the transition, fall back to trying the
	 * legacy port if the new port connection fails.
	 */
	if (( sn = connectsn2( &sin )) == NULL && port == 0 ) {
	    /* try connecting to old non-IANA registered port */
	    sin.sin_port = htons( RADMIND_LEGACY_PORT );
	    sn = connectsn2( &sin );
	}
	if ( sn != NULL ) {
	    return( sn );
	}
    }
    fprintf( stderr, "connection to %s failed: %s\n",
	    inet_ntoa( sin.sin_addr ), strerror( errno ));
    fprintf( stderr, "%s: connection failed\n", host );
    return( NULL );
}

    int
closesn( SNET *sn )
{
    char		*line;
    struct timeval      tv;

    /* Close network connection */
    if ( snet_writef( sn, "QUIT\r\n" ) < 0 ) {
	fprintf( stderr, "QUIT failed: %s\n", strerror( errno ));
	exit( 2 );
    }
    tv = timeout;
    if (( line = snet_getline_multi( sn, logger, &tv )) == NULL ) {
	fprintf( stderr, "close failed: %s\n", strerror( errno ));
	exit( 2 );
    }
    if ( *line != '2' ) {
	perror( line );
	return( -1 );
    }
    if ( snet_close( sn ) != 0 ) {
	fprintf( stderr, "snet_close failed: %s\n", strerror( errno ));
	exit( 2 );
    }
    return( 0 );
}

	char **
get_capabilities( SNET *sn )
{
    char		*line;
    char		temp[ MAXPATHLEN ];
    char		**capa = NULL;
    int			i, ac;
    char		**av;
    struct timeval	tv;

    while( 1 ) {
	tv = timeout;
	if (( line = snet_getline( sn, &tv )) == NULL ) {
	    fprintf( stderr, "connection failed\n" );
	    return( NULL );
	}
	if ( *line != '2' ) {
	    fprintf( stderr, "%s\n", line );
	    return( NULL );
	}
	if ( verbose ) printf( "<<< %s\n", line );
	strcpy( temp, line+4 );
	if (( ac = argcargv( temp, &av )) != 0 ) {
	    if ( strncasecmp( "CAPAbilities", av[0], MIN( 12, strlen( av[0] ))) == 0 ) {
		capa = malloc( sizeof(char *)*ac );
		for ( i = 0; i+1 < ac; i++ ) {
		    capa[i] = strdup( av[i+1] );
		}
		capa[i] = NULL;
	    }
	}
	switch( line[3] ) {
	case ' ':
	    if( !capa ) {
		capa = malloc( sizeof( char * ));
		*capa = NULL;
	    }
	    return ( capa );
	case '-':
	    break;
	default:
	    fprintf ( stderr, "%s\n", line );
	    return ( NULL );
	}
    }
}

#ifdef HAVE_ZLIB
    int
negotiate_compression( SNET *sn, char **capa )
{
    char          	*name = NULL;
    char          	*line;
    int            	type = 0;
    int		   	level = 0;
    struct timeval	tv;

    /* Place compression algorithms in descending order of desirability */
    if ( zlib_level ) { 
	/* walk through capabilities looking for "ZLIB" */
	if ( check_capability( "ZLIB", capa ) == 1 ) {
	    name = "ZLIB";
	    type = SNET_ZLIB;
	    level = zlib_level;
	}

	if ( level == 0 ) {
	    fprintf( stderr, "compression capability mismatch, "
		"compression disabled\n" );
	    return( 0 );
	}
    }

    if ( verbose ) printf( ">>> COMP %s %d\n", name, level );
    snet_writef( sn, "COMP %s %d\r\n", name, level );

    tv = timeout;
    if (( line = snet_getline( sn, &tv )) == NULL ) {
	perror( "snet_getline" );
	return( -1 );
    }
    if( verbose ) printf( "<<< %s\n", line );
    if ( *line != '3' ) {
	fprintf( stderr, "%s\n",  line );
	return( -1 );
    }

    if ( snet_setcompression( sn, type, level ) < 0 ) {
	perror( "snet_setcompression" );
	return( -1 );
    }

    tv = timeout;
    if (( line = snet_getline( sn, &tv )) == NULL ) {
	perror( "snet_getline" );
	return( -1 );
    }
    if( verbose ) printf( "<<< %s\n", line );
    if ( *line != '2' ) {
	fprintf( stderr, "%s\n",  line );
	return( -1 );
    }

    return( 0 );
}

    int
print_stats( SNET *sn )
{
    z_stream		in_stream, out_stream;

    if ( snet_flags( sn ) & SNET_ZLIB ) {
    	in_stream = snet_zistream( sn );
	out_stream = snet_zostream( sn );

#define RATIO(a,b) (((double)(a))/((double)(b)))
	if ( verbose ) {
	    printf( "zlib stats +++ In: %lu:%lu (%.3f:1) "
		"+++ Out: %lu:%lu (%.3f:1)\n",
		in_stream.total_out, in_stream.total_in,
		RATIO( in_stream.total_out, in_stream.total_in ),
		out_stream.total_in, out_stream.total_out,
		RATIO( out_stream.total_in, out_stream.total_out ));
	} else {
	    syslog( LOG_INFO, "zlib stats +++ In: %lu:%lu (%.3f:1) "
		"+++ Out: %lu:%lu (%.3f:1)\n",
		in_stream.total_out, in_stream.total_in,
		RATIO( in_stream.total_out, in_stream.total_in ),
		out_stream.total_in, out_stream.total_out,
		RATIO( out_stream.total_in, out_stream.total_out ));
	    }
    }
    return( 0 );
}
#endif /* HAVE_ZLIB */

/*
 * check_capabilities: check to see if type is a listed capability
 *
 * return codes:
 *      0:      type not in capability list
 *      1:      type in capability list
 */
    int
check_capability( char *type, char **capa )
{
    char **p;

    /* walk through capabilities looking for "REPO" */
    for ( p = capa; *p; p++ ) {
        if ( !strncasecmp( type, *p, MIN( 4, strlen( *p )))) {
            return( 1 );
        }   
    }
    return( 0 );
} 
