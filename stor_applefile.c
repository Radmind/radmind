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

#include "cksum.h"
#include "applefile.h"

extern struct timeval	timeout;
extern struct as_header as_header;
extern struct attrlist  alist;
extern int		verbose;
int			dodots = 0;

    int    
stor_applefile( int dfd, SNET *sn, char *filename )
{
    int		    	    rfd, rc;
    size_t		    asingle_size = 0;
    char	    	    rsrc_path[ MAXPATHLEN ];
    char		    buf[ 8192 ];
    static char		    null_buf[ 32 ] = { 0 };
    struct timeval	    tv;
    struct stat		    r_stp;	    /* for rsrc fork */
    struct stat		    d_stp;	    /* for data fork */
    static struct as_entry  ae_ents[ 3 ] = {	{ ASEID_FINFO, 62, 32 },
						{ ASEID_RFORK, 94, 0 },
						{ ASEID_DFORK, 0, 0 }
					    };
    struct {
	unsigned long	fs_ssize;
	char		fs_fi[ 32 ];
    } fi_struct;

    /* must check for finder info here first */
    if ( getattrlist( filename, &alist, &fi_struct, sizeof( fi_struct ),
		FSOPT_NOFOLLOW ) != 0 ) {
	fprintf( stderr, "Non-HFS+ filesystem\n" );
	goto error1;
    }

    if ( memcmp( fi_struct.fs_fi, null_buf, sizeof( null_buf )) == 0 ) {
	goto error1;
    }

    tv = timeout;

    if ( fstat( dfd, &d_stp ) != 0 ) {
	perror( filename );
	goto error1;
    }

    if ( snprintf( rsrc_path, MAXPATHLEN, "%s%s", filename, _PATH_RSRCFORKSPEC )
		> MAXPATHLEN ) {
	fprintf( stderr, "%s%s: path too long\n", filename,
		_PATH_RSRCFORKSPEC );
	goto error1;
    }

    if (( rfd = open( rsrc_path, O_RDONLY )) < 0 ) {
	perror( rsrc_path );
	goto error1;
    }

    if 	( fstat( rfd, &r_stp ) != 0 ) {
	/* if there's no rsrc fork, but there is finder info,
	 * assume zero length rsrc fork.
	 */
	if ( errno == ENOENT ) {
	    ae_ents[ AS_RFE ].ae_length = 0;
    	} else {
	    perror( rsrc_path );
	    goto error2;
	}
    } else {
    	ae_ents[ AS_RFE ].ae_length = ( int )r_stp.st_size;
    }

    ae_ents[ AS_DFE ].ae_offset = 
	( ae_ents[ AS_RFE ].ae_offset + ae_ents[ AS_RFE ].ae_length );
    ae_ents[ AS_DFE ].ae_length = ( int )d_stp.st_size;

    /* calculate total applesingle file size */
    asingle_size = ( AS_HEADERLEN + ( 3 * sizeof( struct as_entry ))
		+ sizeof( fi_struct.fs_fi ) + ae_ents[ AS_RFE ].ae_length
		+ ae_ents[ AS_DFE ].ae_length );

    /* tell server how much data to expect */
    tv = timeout;
    if ( snet_writef( sn, "%d\r\n", ( int )asingle_size ) == NULL ) {
        perror( "snet_writef" );
        goto error2;
    }
    if ( verbose ) printf( ">>> %d\n", ( int )asingle_size );

    /* snet write applesingle header to server */
    tv = timeout;
    if ( snet_write( sn, ( char * )&as_header, AS_HEADERLEN, &tv ) !=
		AS_HEADERLEN  ) {
	perror( "snet_write" );
	goto error2;
    }
    if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }

    /* snet write header entries */
    tv = timeout;
    if ( snet_write( sn, ( char * )&ae_ents,
		( 3 * sizeof( struct as_entry )), &tv )
		!= ( 3 * sizeof( struct as_entry ))) {
	perror( "snet_write" );
	goto error2;
    }
    if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }

    /* snet_write finder info data to server */
    tv = timeout;
    if ( snet_write( sn, fi_struct.fs_fi, sizeof( fi_struct.fs_fi ), &tv ) !=
		sizeof( fi_struct.fs_fi )) {
	perror( "snet_write" );
	goto error2;
    }
    if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }

    /* snet_write rsrc fork data to server */
    if ( rfd >= 0 ) {
	while (( rc = read( rfd, buf, sizeof( buf ))) > 0 ) {
	    tv = timeout;
	    if ( snet_write( sn, buf, rc, &tv ) != rc ) {
		perror( "snet_write" );
		goto error2;
	    }
	    if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }
	}
	if ( close( rfd ) < 0 ) {
	    perror( "close rfd" );
	    exit( 1 );
	}
    }

    /* snet_write data fork to server */
    while (( rc = read( dfd, buf, sizeof( buf ))) > 0 ) {
	tv = timeout;
	if ( snet_write( sn, buf, rc, &tv ) != rc ) {
	    perror( "snet_write" );
	    goto error2;
	}
    	if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }
    }
    /* dfd is closed in main() of lcreate.c */

    return( 0 ); 

error1:
    return( -1 );
error2:
    if ( close( rfd ) < 0 ) {
	perror( rsrc_path );
	exit( 1 );
    }
    return( -1 );
}
#else !__APPLE__

#include <sys/types.h>
#include <stdio.h>
#include <snet.h>

#include "applefile.h"

    int
stor_applefile( int dfd, SNET *sn, char *filename )
{
    return( -1 );
}
#endif __APPLE__
