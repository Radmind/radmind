#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sha.h>
#include <sys/ddi.h>
#include <unistd.h>

#include "snet.h"
#include "code.h"
#include "base64.h"
#include "download.h"

#define MIN(X, Y) ((X) < (Y) ? (X) : (Y))

extern void            (*logger)( char * );
extern struct timeval  	timeout;
extern int 		linenum;
extern int		verbose;
extern int		chksum;

    int
download( SNET *sn, char *transcript, char *path, char *chksumval ) 
{
    struct timeval      tv;
    char 		*line;
    char                *temppath;
    unsigned int	rr;
    int			fd;
    size_t              size;
    char                buf[ 8192 ]; 
    unsigned char	md[ SHA_DIGEST_LENGTH ];
    unsigned char	mde[ SZ_BASE64_E( sizeof( md ) ) ];
    SHA_CTX		sha_ctx;

    if ( chksum && ( strcmp( chksumval, "-" ) == 0 ) ) {
	perror( "Chksum not in transcript" );
	return( -1 );
    }

    /* Connect to server */ 
    if( snet_writef( sn, "RETR FILE %s %s\n", transcript,
	    encode( path ) ) == NULL ) {
	perror( "snet_writef" );
	return( -1 );
    }

    if ( verbose ) printf( ">>> RETR FILE %s %s\n", transcript,
	encode( path ) );

    tv = timeout;
    if ( ( line = snet_getline_multi( sn, logger, &tv ) ) == NULL ) {
	perror( "snet_getline_multi" );
	return( -1 );
    }

    if ( verbose ) printf( "<<< %s\n", line );

    if ( *line != '2' ) {
	perror( line );
	return( -1 );
    }

    /*Create temp file name*/
    temppath = (char *)malloc( MAXPATHLEN );
    if ( temppath == NULL ) {
	perror( "malloc" );
	return ( -1 );
    }
    sprintf( temppath, "%s.radmind.%i", path, rand() );

    /* Open file */
    if ( ( fd = open( temppath, O_WRONLY | O_CREAT | O_EXCL, 0 ) ) < 0 ) {
	perror( temppath );
	return( -1 );
    }
    tv = timeout;
    if ( ( line = snet_getline( sn, &tv ) ) == NULL ) {
	perror( "snet_getline 1" );
	goto error;
    }

    size = atoi( line );
    if ( verbose ) printf( "<<< %i\n", size );
    if ( verbose ) printf( "<<< " );

    if ( chksum ) SHA1_Init( &sha_ctx );

    /* Get file from server */
    while ( size > 0 ) {
	tv = timeout;
	if ( ( rr = snet_read( sn, buf, (int)MIN( sizeof( buf ), size ),
		&tv ) ) <= 0 ) {
	    perror( "snet_read" );
	    goto error;
	}
	if ( chksum ) SHA1_Update( &sha_ctx, buf, rr );
	if ( write( fd, buf, (size_t)rr ) != rr ) {
	    perror( temppath );
	    goto error;
	}
	if ( verbose ) printf( "..." );
	size -= rr;
    }

    if ( close( fd ) != 0 ) {
	perror( temppath );
	goto error;
    }
    if ( chksum ) {
	SHA1_Final( md, &sha_ctx );
	base64_e( md, sizeof( md ), mde );
	if ( strcmp( chksumval, mde ) != 0 ) {
	    perror( "Chksum failed" );
	    goto error;
	}
    }
    if ( rename( temppath, path ) != 0 ) {
	perror( path );
	goto error;
    }

    free( temppath );

    if ( size != 0 ) {
	perror( "Did not write correct number of bytes" );
	return( -1 );
    }

    tv = timeout;
    if ( ( line = snet_getline( sn, &tv ) ) == NULL ) {
	perror( "snet_getline" );
	return( -1 );
    }
    if ( strcmp( line, "." ) != 0 ) {
	perror( line );
	return( -1 );
    }

    return ( 0 );

error:
    close( fd );
    unlink( temppath );
    free ( temppath );
    return ( -1 );

}
