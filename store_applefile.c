#ifdef __APPLE__
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/paths.h>
#include <sys/attr.h>
#include <sys/uio.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include <snet.h>

#include "store_applefile.h"
#include "chksum.h"
#include "applefile.h"

extern struct timeval	timeout;
extern struct as_header as_header;
extern int		verbose;

/*
 * applesingle format:
 *  header:
 *	-magic number (4 bytes)
 *	-version number (4 bytes)
 *	-filler (16 bytes)
 *	-number of entries (2 bytes)
 *	-x number of entries, with this format:
 *	    id (4 bytes)
 *	    offset (4 bytes)
 *	    length (4 bytes)
 *  data:
 *	-finder info
 *	-rsrc fork
 *	-data fork
 */

    int    
store_applefile( const char *path, int afd, SNET *sn, int dodots )
{
    int		    	rfd, r_cc, d_cc, d_size, r_size, err, has_rsrc = 0;
    size_t		asingle_size = 0;
    const char	    	*rsrc_suffix = _PATH_RSRCFORKSPEC; /* sys/paths.h */
    char	    	rsrc_path[ MAXPATHLEN ];
    char		data_buf[ 8192 ];
    char		finfo_buf[ 32 ] = { 0 };
    struct stat		r_stp;	    /* for rsrc fork */
    struct stat		d_stp;	    /* for data fork */
    struct as_entry	as_entry_finfo = { ASEID_FINFO, 62, 32 };
    struct as_entry	as_entry_rfork = { ASEID_RFORK, 94, 0 };
    struct as_entry	as_entry_dfork = { ASEID_DFORK, 0, 0 };
    struct timeval tv;
    extern struct timeval timeout;

    tv = timeout;

    if ( snprintf( rsrc_path, MAXPATHLEN, "%s%s", path, rsrc_suffix )
		> MAXPATHLEN ) {
	fprintf( stderr, "%s%s: path too long\n", path, rsrc_suffix );
    }

    if ( lstat( path, &d_stp ) != 0 ) {
	perror( path );
	exit( 1 );
    }

    if 	( lstat( rsrc_path, &r_stp ) != 0 ) {
	/* if there's no rsrc fork, but there is finder info,
	 * assume zero length rsrc fork.
	 */
	if ( errno == ENOENT ) {
	    r_size = 0;
    	} else {
	    perror( rsrc_path );
	    exit( 1 );
	}
    } else {
	has_rsrc++;
    	r_size = ( int )r_stp.st_size;
    }

    d_size = ( int )d_stp.st_size;

    /* open rsrc fork */
    if ( has_rsrc ) {
	if (( rfd = open( rsrc_path, O_RDONLY )) < 0 ) {
	    perror( rsrc_path );
	    exit( 1 );
	}
    }

    /* rsrc fork header entry */
    as_entry_rfork.ae_length = r_size;

    /* data fork header entry */
    as_entry_dfork.ae_offset = 
	( as_entry_rfork.ae_offset + as_entry_rfork.ae_length );
    as_entry_dfork.ae_length = d_size;

    /* calculate total applesingle file size */
    asingle_size = ( AS_HEADERLEN + ( 3 * sizeof( struct as_entry ))
		+ sizeof( finfo_buf ) + r_size + d_size );

    /* tell server how much data to expect */
    if ( snet_writef( sn, "%d\r\n", ( int )asingle_size ) == NULL ) {
        perror( "snet_writef" );
        return( -1 );
    }
    if ( verbose ) printf( ">>> %d\n", ( int )asingle_size );

    /* snet write applesingle header to server */
    if ( snet_write( sn, ( char * )&as_header, AS_HEADERLEN, &tv ) !=
		AS_HEADERLEN  ) {
	perror( "snet_write" );
	return( -1 );
    }
    /* is this okay? */
    if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }

    /* snet write header entry for finder info */
    if ( snet_write( sn, ( char * )&as_entry_finfo,
		( int )sizeof( as_entry_finfo ), &tv )
		!= ( int )sizeof( as_entry_finfo )) {
	perror( "snet_write" );
	return( -1 );
    }
    if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }

    /* snet write header entry for rsrc fork */
    if ( snet_write( sn, ( char * )&as_entry_rfork,
		( int )sizeof( as_entry_rfork ), &tv )
		!= ( int )sizeof( as_entry_rfork )) {
	perror( "snet_write" );
	return( -1 );
    }
    if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }

    /* snet write header entry for data fork */
    if ( snet_write( sn, ( char * )&as_entry_dfork,
		( int )sizeof( as_entry_dfork ), &tv )
		!= ( int )sizeof( as_entry_dfork )) {
	perror( "snet_write" );
    	return( -1 );
    }
    if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }

    err = chk_for_finfo( path, finfo_buf );
    if ( err ) {
	fprintf( stderr, "Non-hfs system\n" );
	return( -1 );
    }

    /* snet_write finder info data to server */
    if ( snet_write( sn, finfo_buf, ( int )sizeof( finfo_buf ), &tv ) !=
		sizeof( finfo_buf )) {
	perror( "snet_write" );
	return( -1 );
    }
    if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }

    /* snet_write rsrc fork data to server */
    if ( has_rsrc ) {
	while (( r_cc = read( rfd, data_buf, sizeof( data_buf ))) > 0 ) {
	    if ( snet_write( sn, data_buf, r_cc, &tv ) != r_cc ) {
		perror( "snet_write" );
		return( -1 );
	    }
	    if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }
	}
    }

    /* snet_write data fork to server */
    while (( d_cc = read( afd, data_buf, sizeof( data_buf ))) > 0 ) {
	if ( snet_write( sn, data_buf, d_cc, &tv ) != d_cc ) {
	    perror( "snet_write" );
	    return( -1 );
	}
    	if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }
    }

    if ( has_rsrc ) {
	if ( close( rfd ) < 0 ) {
	    perror( "close rfd" );
	    exit( 1 );
	}
    }

    if ( close( afd ) < 0 ) {
	perror( "close afd" );
	exit( 1 );
    }

    /* free all the alloc'd memory */
   
    return( 0 ); 
}

#else !__APPLE__

#include <sys/time.h>

#include <snet.h>

#include "store_applefile.h"

    int    
store_applefile( const char *path, int afd, SNET *sn, int dodots )
{
    return( -1 );
}

#endif __APPLE__
