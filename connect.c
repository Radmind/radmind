/*
 * Copyright (c) 2003 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/sha.h>

#include <snet.h>

#include "applefile.h"
#include "cksum.h"
#include "connect.h"

extern void            (*logger)( char * );
extern int              verbose;
struct timeval          timeout = { 60, 0 };
extern int		errno;
extern SSL_CTX  	*ctx;

    static SNET *
connectsn2( struct sockaddr_in *sin )
{
    int			s;
    char		*line;
    struct timeval      tv;
    SNET                *sn = NULL; 

    if (( s = socket( PF_INET, SOCK_STREAM, NULL )) < 0 ) {
	perror( "socket" );
	exit( 2 );
    }
    if ( verbose ) printf( "trying %s... ", inet_ntoa( sin->sin_addr ));
    if ( connect( s, (struct sockaddr *)sin,
	    sizeof( struct sockaddr_in )) != 0 ) {
	if ( verbose ) printf( "failed: %s\n", strerror( errno ));
	fprintf( stderr, "connection to %s failed: %s\n",
		inet_ntoa( sin->sin_addr ), strerror( errno ));
	(void)close( s );
	return( NULL );
    }
    if ( verbose ) printf( "success!\n" );
    if (( sn = snet_attach( s, 1024 * 1024 )) == NULL ) {
	perror( "snet_attach" );
	exit( 2 );
    }
    tv = timeout;
    if (( line = snet_getline_multi( sn, logger, &tv )) == NULL ) {
	fprintf( stderr, "connection to %s failed\n",
		inet_ntoa( sin->sin_addr ));
	snet_close( sn );
	return( NULL );
    }
    if ( *line != '2' ) {
	fprintf( stderr, "%s\n", line );
	snet_close( sn );
	return( NULL );
    }

    return( sn );
}

    SNET *
connectsn( char *host, int port )
{
    int			i;
    struct hostent      *he;
    struct sockaddr_in  sin;
    SNET                *sn = NULL; 

    memset( &sin, 0, sizeof( struct sockaddr_in ));
    sin.sin_family = AF_INET;
    sin.sin_port = port;

    /*
     * this code should be enabled only to deal with bugs in
     * the gethostbyname() routine
     */
    if (( sin.sin_addr.s_addr = inet_addr( host )) != -1 ) {
	return( connectsn2( &sin ));
    }

    if (( he = gethostbyname( host )) == NULL ) {
	fprintf( stderr, "%s: Unknown host\n", host );
	return( NULL );
    }
    
    for ( i = 0; he->h_addr_list[ i ] != NULL; i++ ) {
	memcpy( &sin.sin_addr.s_addr, he->h_addr_list[ i ],
		(unsigned int)he->h_length );
	if (( sn = connectsn2( &sin )) != NULL ) {
	    return( sn );
	}
    }
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
