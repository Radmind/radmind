/* check file for AS magic number. If AS, decode. */

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/paths.h>
#include <sys/attr.h>
#include <string.h>
#include <snet.h>

#include <sha.h>

#include "applefile.h"
#include "chksum.h"
#include "base64.h"

extern struct timeval	timeout;
extern int		verbose;
extern int              chksum;

void            (*logger)( char * );

char			null_buf[ 32 ] = { 0 };
struct attrlist		alist = {
    ATTR_BIT_MAP_COUNT,
    0,
    ATTR_CMN_FNDRINFO,
    0,
    0,
    0,
    0,
};

struct as_header	as_header = {
    0x00051600,
    0x00020000,
    {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    },
    NUM_ENTRIES,
};

    int
retr_applefile( SNET *sn, char *pathdesc, char *path, char *location,
	char *chksumval, char *temppath, int linenum )
{
    int			rc;
    int			ofd, rsrcfd, rsize, as_cc;
    int			dodots = 0;
    size_t		size;
    unsigned char	finfo[ 32 ];
    char		as_buf[ 8192 ];
    char		rsrc_path[ MAXPATHLEN ];
    const char		*rsrc_suffix = _PATH_RSRCFORKSPEC;
    struct as_header	as_dest;
    struct as_entry	ae_finfo;
    struct as_entry	ae_rfork;
    struct as_entry	ae_dfork;
    struct timeval	tv = timeout;
    char		*line;
    unsigned char       md[ SHA_DIGEST_LENGTH ];
    unsigned char       mde[ SZ_BASE64_E( sizeof( md )) ];
    SHA_CTX             sha_ctx;

    memset( &as_dest, '\0', ( int )sizeof( struct as_header ));
    memset( &ae_finfo, '\0', ( int )sizeof( struct as_entry ));
    memset( &ae_rfork, '\0', ( int )sizeof( struct as_entry ));
    memset( &ae_dfork, '\0', ( int )sizeof( struct as_entry ));

    if ( chksum ) {
	if ( strcmp( chksumval, "-" ) == 0 ) {
	    fprintf( stderr, "line %d: Chksum not listed\n", linenum );
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
    if (( line = snet_getline_multi( sn, logger, &tv )) == NULL ) {
        fprintf( stderr, "snet_getline_multi failed\n" );
        return( -1 );
    }

    if ( *line != '2' ) {
        fprintf( stderr, "%s\n", line );
        return( -1 );
    }

    /* Create temp file name */
    if ( location == NULL ) {
        if ( snprintf( temppath, MAXPATHLEN, "%s.radmind.%i",
                path, getpid() ) > MAXPATHLEN ) {
            fprintf( stderr, "%s.radmind.%i: too long", path,
                    (int)getpid() );
            exit( 1 );
        }
    } else {
        if ( snprintf( temppath, MAXPATHLEN, "%s", location ) > MAXPATHLEN ) {
            fprintf( stderr, "%s: too long", path );
            exit( 1 );
        }
    }

    /* Get file size from server */
    tv = timeout;
    if (( line = snet_getline( sn, &tv )) == NULL ) {
        fprintf( stderr, "snet_getline" );
        exit( 1 );
    }
    size = atoi( line );
    if ( verbose ) printf( "<<< %d\n<<< ", size );

    /*
     * If output is not a tty, don't bother with the dots.
     */
    if ( verbose && isatty( fileno( stdout ))) {
        dodots = 1;
    }

    tv = timeout;
    /* read header to determine if file is encoded in applesingle */
    if (( as_cc = snet_read( sn, ( char * )&as_dest, AS_HEADERLEN, &tv ))
		<= 0 ) {
	perror( "snet_read" ); 
	exit( 1 );
    }
    if ( chksum ) SHA1_Update( &sha_ctx, ( char * )&as_dest, (size_t)as_cc );
    if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }
    size -= as_cc;

    if ( as_dest.ah_magic != AS_MAGIC 
	    || as_dest.ah_version != AS_VERSION 
	    || as_dest.ah_num_entries != NUM_ENTRIES ) {
	fprintf( stderr, "%s is not a radmind AppleSingle file.\n", path );
	exit( 1 );
    }

    /* read finder info header entry */
    tv = timeout;
    if (( as_cc = snet_read( sn, ( char * )&ae_finfo,
		sizeof( struct as_entry ), &tv )) <= 0 ) {
	perror( "snet_read" );
	exit( 1 );
    }
    if ( chksum ) SHA1_Update( &sha_ctx, ( char * )&ae_finfo, (size_t)as_cc );
    if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }

    size -= as_cc;

    /* read rsrc fork header entry */
    tv = timeout;
    if (( as_cc = snet_read( sn, ( char * )&ae_rfork,
		sizeof( struct as_entry ), &tv )) <= 0 ) {
	perror( "snet_read" );
	exit( 1 );
    }
    if ( chksum ) SHA1_Update( &sha_ctx, ( char * )&ae_rfork, (size_t)as_cc );
    if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }

    size -= as_cc;

    /* read data fork header entry */
    tv = timeout;
    if (( as_cc = snet_read( sn, ( char * )&ae_dfork,
		sizeof( struct as_entry ), &tv )) <= 0 ) {
	perror( "snet_read" );
	exit( 1 );
    }
    if ( chksum ) SHA1_Update( &sha_ctx, ( char * )&ae_dfork, (size_t)as_cc );
    if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }

    size -= as_cc;

    if (( ofd = open( temppath, O_CREAT | O_EXCL | O_WRONLY, 0666 )) < 0 ) {
	perror( temppath );
	exit( 1 );
    }

    tv = timeout;
    if (( as_cc = snet_read( sn, finfo, sizeof( finfo ), &tv )) < 0 ) {
	perror( "snet_read" );
	exit( 1 );
    }
    if ( chksum ) SHA1_Update( &sha_ctx, finfo, (size_t)as_cc );
    if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }
    size -= as_cc;

    if ( snprintf( rsrc_path, MAXPATHLEN, "%s%s", temppath, rsrc_suffix ) >
		MAXPATHLEN ) {
	fprintf( stderr, "%s%s: path too long\n", temppath, rsrc_suffix );
	exit( 1 );
    }
        
    if (( rsrcfd = open( rsrc_path, O_WRONLY, 0 )) < 0 ) {
        perror( rsrc_path );
        exit( 1 );
    };  

    rsize = ae_rfork.ae_length;
   
    while ( rsize > 0 ) { 
	tv = timeout;
	if (( as_cc = snet_read( sn, as_buf, ( int )MIN( sizeof( as_buf ),
		rsize ), &tv )) < 0 ) {
	    perror( "snet_read" );
	    exit( 1 );
	}

	if (( write( rsrcfd, as_buf, ( unsigned int )as_cc )) != as_cc ) {
	    perror( "rsrcfd write" );
	    exit( 1 );
	}
	if ( chksum ) SHA1_Update( &sha_ctx, as_buf, (size_t)as_cc );
	if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }
	rsize -= as_cc;
    }

    if ( rsize != 0 ) {
	fprintf( stderr, "Didn't write correct number of bytes to rsrc fork" );
	exit( 1 );
    }
    
    if ( close( rsrcfd ) < 0 ) {
	perror( "close rsrcfd" );
	exit( 1 );
    }

    size -= ae_rfork.ae_length;

    /* write data fork to file */
    while ( size > 0 ) {
    	tv = timeout;
    	if (( as_cc = snet_read( sn, as_buf, (int)MIN( sizeof( as_buf ), size),
		&tv )) <= 0 ) {
	    perror( "snet_read" );
	    exit( 1 );
	}

	if ( write( ofd, as_buf, ( unsigned int )as_cc ) != as_cc ) {
	    perror( "ofd write" );
	    exit( 1 );
	}

	if ( chksum ) SHA1_Update( &sha_ctx, as_buf, (size_t)as_cc );
	if ( dodots ) { putc( '.', stdout ); fflush( stdout); }
	size -= as_cc;
    }
    if ( verbose ) printf( "\n" );

    if ( close( ofd ) < 0 ) {
	perror( "close ofd" );
	exit( 1 );
    }

    if (( rc = setattrlist( temppath, &alist, finfo, 32, FSOPT_NOFOLLOW ))) {
	perror( "setattrlist" );
	exit( 1 );
    }

    tv = timeout;
    if ( ( line = snet_getline( sn, &tv ) ) == NULL ) {
        fprintf( stderr, "snet_getline" );
        exit( 1 );
    }
    if ( strcmp( line, "." ) != 0 ) {
        fprintf( stderr, "%s", line );
        exit( 1 );
    }
    if ( verbose ) printf( "<<< .\n" );

    /* Chksum file */
    if ( chksum ) {
        SHA1_Final( md, &sha_ctx );
        base64_e( md, sizeof( md ), mde );
        if ( strcmp( chksumval, mde ) != 0 ) {
            fprintf( stderr, "checksum failed: %s\n", path );
            exit( 1 );
        }
    }

    return( 0 );
}

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
chk_for_finfo( const char *path, char *finfo )
{
    int			err = 0;
    struct {
	unsigned long	ssize;
	char		finfo_buf[ 32 ];
    } finfo_struct;

    memset( finfo_struct.finfo_buf, 0, sizeof( finfo_struct.finfo_buf ));
    memset( null_buf, 0, sizeof( null_buf ));

    if (( err = getattrlist( path, &alist, &finfo_struct,sizeof( finfo_struct ),
		FSOPT_NOFOLLOW ))) {
	return( err );
    }

    if ( memcmp( finfo_struct.finfo_buf, null_buf, sizeof( null_buf )) == 0 ) {
	err++;
    } else {
    	memcpy( finfo, finfo_struct.finfo_buf,
		sizeof( finfo_struct.finfo_buf ));
    }

    return( err );
}
