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
#include "chksum.h"

#define MIN(X, Y) ((X) < (Y) ? (X) : (Y))

extern void            (*logger)( char * );
extern struct timeval  	timeout;
extern int 		linenum;
extern int		verbose;
extern int		chksum;

    int
get( SNET *sn, char *pathdesc, char *path, char *chksumval )
{
    char 	*temppath;

    if ( ( temppath = download( sn, pathdesc, path, chksumval ) ) == NULL ) {
	fprintf( stderr, "%s: unable to download\n", path );
	return( 1 );
    }
    if ( rename( temppath, path ) != 0 ) {
	perror( temppath );
	return( 1 );
    }
    free( temppath );

    return( 0 );
}

/*
 * Download requests path from sn and writes it to disk.  The path to
 * this new file is returned upon success and must be freed by the caller.
 * NULL is returned otherwise.
 */

    char *
download( SNET *sn, char *pathdesc, char* path, char *chksumval ) 
{
    struct timeval      tv;
    char 		*line;
    char                *temppath;
    unsigned int	rr;
    int			fd;
    size_t              size;
    char                buf[ 8192 ]; 
    unsigned char	chksumcalc[ 29 ];

    if ( chksum && ( strcmp( chksumval, "-" ) == 0 ) ) {
	fprintf( stderr, "Chksum not in transcript" );
	return( NULL );
    }

    /* Connect to server */ 
    if( snet_writef( sn, "RETR %s\n", pathdesc ) == NULL ) {
	fprintf( stderr, "snet_writef" );
	return( NULL );
    }

    if ( verbose ) printf( ">>> RETR %s\n", pathdesc );

    tv = timeout;
    if ( ( line = snet_getline_multi( sn, logger, &tv ) ) == NULL ) {
	fprintf( stderr, "snet_getline_multi" );
	return( NULL );
    }

    if ( *line != '2' ) {
	fprintf( stderr, "%s", line );
	return( NULL );
    }

    /*Create temp file name*/
    temppath = (char *)malloc( MAXPATHLEN );
    if ( temppath == NULL ) { perror( "malloc" );
	return ( NULL );
    }
    srand( (unsigned int)getpid() );
    sprintf( temppath, "%s.radmind.%i", path, rand() );

    /* Open file */
    if ( ( fd = open( temppath, O_RDWR | O_CREAT | O_EXCL, 0666 ) ) < 0 ) {
	perror( temppath );
	goto error3;
    }
    tv = timeout;
    if ( ( line = snet_getline( sn, &tv ) ) == NULL ) {
	fprintf( stderr, "snet_getline" );
	goto error;
    }
    size = atoi( line );

    if ( verbose ) printf( "<<< " );

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
	if ( verbose ) printf( "." );
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
    if ( chksum ) {
	if ( verbose ) printf( "*** chksum " );

	if ( lseek( fd, 0, SEEK_SET ) != 0 ) {
	    perror( "lseek" );
	    goto error;
	}

	if( do_chksum_fd( fd, chksumcalc ) != 0 ) {
	    fprintf( stderr, "do_chksum_fd failed" );
	    goto error;
	}

	if ( strcmp( chksumval, chksumcalc ) != 0 ) {
	    fprintf( stderr, " mismatch" );
	    goto error;
	}
	if ( verbose ) printf( "match\n" );
    }
    if ( close( fd ) != 0 ) {
	perror( path );
	goto error2;
    }
    return ( temppath );

error:
    close( fd );
error2:
    unlink( temppath );
error3:
    free ( temppath );
    return ( NULL );

}
