#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <fcntl.h>
#include <snet.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <sha.h>

#include "retr.h"
#include "cksum.h"
#include "base64.h"

#ifdef sun
#define MIN(X, Y) ((X) < (Y) ? (X) : (Y))
#endif sun

extern void            (*logger)( char * );
extern struct timeval  	timeout;
extern int 		linenum;
extern int		verbose;
extern int		dodots;
extern int		cksum;

/*
 * Download requests path from sn and writes it to disk.  The path to
 * this new file is returned via temppath which must be 2 * MAXPATHLEN.
 */

    int 
retr( SNET *sn, char *pathdesc, char *path, char *cksumval, char *temppath ) 
{
    struct timeval      tv;
    char 		*line;
    int			fd;
    size_t              size;
    unsigned char       buf[ 8192 ]; 
    ssize_t             rr;
    unsigned char       md[ SHA_DIGEST_LENGTH ];
    unsigned char       mde[ SZ_BASE64_E( sizeof( md )) ];
    SHA_CTX             sha_ctx;


    if ( cksum ) {
	if ( strcmp( cksumval, "-" ) == 0 ) {
	    fprintf( stderr, "line %d: cksum not listed\n", linenum);
	    return( -1 );
	}
	SHA1_Init( &sha_ctx );
    }

    if( snet_writef( sn, "RETR %s\n", pathdesc ) == NULL ) {
	fprintf( stderr, "snet_writef failed\n" );
	return( -1 );
    }
    if ( verbose ) printf( ">>> RETR %s\n", pathdesc );

    tv = timeout;
    if ( ( line = snet_getline_multi( sn, logger, &tv ) ) == NULL ) {
	fprintf( stderr, "snet_getline_multi failed\n" );
	return( -1 );
    }

    if ( *line != '2' ) {
	fprintf( stderr, "%s\n", line );
	return( -1 );
    }

    /*Create temp file name*/
    if ( snprintf( temppath, MAXPATHLEN, "%s.radmind.%i",
	    path, getpid() ) > MAXPATHLEN ) {
	fprintf( stderr, "%s.radmind.%i: too long", path,
		(int)getpid() );
	goto error3;
    }

    /* Open file */
    if ( ( fd = open( temppath, O_RDWR | O_CREAT | O_EXCL, 0666 ) ) < 0 ) {
	perror( temppath );
	goto error3;
    }

    /* Get file size from server */
    tv = timeout;
    if ( ( line = snet_getline( sn, &tv ) ) == NULL ) {
	fprintf( stderr, "snet_getline" );
	goto error;
    }
    size = atoi( line );
    if ( verbose ) printf( "<<< %d\n<<< ", size );

    /* Get file from server */
    while ( size > 0 ) {
	tv = timeout;
	if ( ( rr = snet_read( sn, buf, (int)MIN( sizeof( buf ), size ),
		&tv ) ) <= 0 ) {
	    fprintf( stderr, "snet_read" );
	    goto error;
	}
	if ( write( fd, buf, (size_t)rr ) != rr ) {
	    perror( temppath );
	    goto error;
	}
	if ( cksum ) {
	    SHA1_Update( &sha_ctx, buf, (size_t)rr );
	}
	if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }
	size -= rr;
    }
    if ( verbose ) printf( "\n" );

    if ( size != 0 ) {
	fprintf( stderr, "Did not write correct number of bytes" );
	goto error;
    }
    tv = timeout;
    if ( ( line = snet_getline( sn, &tv ) ) == NULL ) {
	fprintf( stderr, "snet_getline" );
	goto error;
    }
    if ( strcmp( line, "." ) != 0 ) {
	fprintf( stderr, "%s", line );
	goto error;
    }
    if ( verbose ) printf( "<<< .\n" );

    /* cksum file */
    if ( cksum ) {
	SHA1_Final( md, &sha_ctx );
	base64_e( md, sizeof( md ), mde );
	if ( strcmp( cksumval, mde ) != 0 ) {
	    fprintf( stderr, "checksum failed: %s\n", path );
	    goto error;
	}
    }
    if ( close( fd ) != 0 ) {
	perror( path );
	goto error2;
    }

    return ( 0 );

error:
    close( fd );
error2:
    unlink( temppath );
error3:
    return ( -1 );
}
