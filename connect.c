/*
 * Copyright (c) 2002 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <openssl/sha.h>
#include <snet.h>

#include "applefile.h"
#include "cksum.h"
#include "connect.h"

extern void            (*logger)( char * );
extern int              verbose;
struct timeval          timeout = { 10 * 60, 0 };
extern int		errno;

    SNET *
connectsn( char *host, int port )
{
    int			i, s;
    char		*line;
    struct timeval      tv;
    struct hostent      *he;
    struct sockaddr_in  sin;
    SNET                *sn = NULL; 


    /* Make network connection */
    if (( he = gethostbyname( host )) == NULL ) {
	fprintf( stderr, "%s: Unkown host\n", host );
	exit( 1 );
    }
    
    for ( i = 0; he->h_addr_list[ i ] != NULL; i++ ) {
	if (( s = socket( PF_INET, SOCK_STREAM, NULL )) < 0 ) {
	    perror( host );
	    exit( 1 );
	}
	memset( &sin, 0, sizeof( struct sockaddr_in ) );
	sin.sin_family = AF_INET;
	sin.sin_port = port;
	memcpy( &sin.sin_addr.s_addr, he->h_addr_list[ i ],
	    ( unsigned int)he->h_length );
	if ( verbose ) printf( "trying %s... ",
		inet_ntoa( *( struct in_addr *)he->h_addr_list[ i ] ) );
	if ( connect( s, ( struct sockaddr *)&sin,
		sizeof( struct sockaddr_in ) ) != 0 ) {
	    if ( verbose ) printf( "failed: %s\n", strerror( errno ));
	    (void)close( s );
	    continue;
	}
	if ( verbose ) printf( "success!\n" );
	if ( ( sn = snet_attach( s, 1024 * 1024 ) ) == NULL ) {
	    fprintf( stderr, "connection to %s failed: %s\n", host,
		strerror( errno ));
	    continue;
	}
	tv = timeout;
	if ( ( line = snet_getline_multi( sn, logger, &tv) ) == NULL ) {
	    fprintf( stderr, "connection to %s failed: %s\n", host,
		strerror( errno ));
	    snet_close( sn );
	    continue;
	}
	if ( *line !='2' ) {
	    fprintf( stderr, "%s\n", line);
	    snet_close( sn );
	    continue;
	}
	break;
    }
    if ( he->h_addr_list[ i ] == NULL ) {
	fprintf( stderr, "%s: connection failed\n", host );
	exit( 1 );
    }
    return( sn );
}


    int
closesn( SNET *sn )
{
    char		*line;
    struct timeval      tv;

    /* Close network connection */
    if ( snet_writef( sn, "QUIT\r\n" ) < 0 ) {
	fprintf( stderr, "close failed: %s\n", strerror( errno ));
	exit( 1 );
    }
    tv = timeout;
    if ( ( line = snet_getline_multi( sn, logger, &tv ) ) == NULL ) {
	fprintf( stderr, "close failed: %s\n", strerror( errno ));
	exit( 1 );
    }
    if ( *line != '2' ) {
	perror( line );
	return( -1 );
    }
    if ( snet_close( sn ) != 0 ) {
	fprintf( stderr, "close failed: %s\n", strerror( errno ));
	exit( 1 );
    }
    return( 0 );
}
