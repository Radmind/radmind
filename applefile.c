#ifdef __APPLE__
/* check file for AS magic number. If AS, decode. */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#ifdef __APPLE__
#include <sys/paths.h>
#include <sys/attr.h>
#endif __APPLE__
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <snet.h>

#include <sha.h>

#include "applefile.h"
#include "cksum.h"
#include "base64.h"

extern int		verbose;
extern int		dodots;
extern int              cksum;
void            	(*logger)( char * );
extern struct timeval	timeout;

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
    AS_MAGIC,
    AS_VERSION,
    {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    },
    AS_NENTRIES,
};

    int
retr_applefile( SNET *sn, char *pathdesc, char *path, char *cksumval,
    char *temppath, int linenum )
{
    int			dfd, rfd, rc;
    size_t		size, rsize;
    char		finfo[ 32 ];
    char		buf[ 8192 ];
    char		rsrc_path[ MAXPATHLEN ];
    char		*line;
    struct as_header	ah;
    struct as_entry	ae_ents[ 3 ]; 
    struct timeval	tv;
    unsigned char       md[ SHA_DIGEST_LENGTH ];
    unsigned char       mde[ SZ_BASE64_E( sizeof( md )) ];
    SHA_CTX             sha_ctx;

    if ( cksum ) {
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

    /* Get file size from server */
    tv = timeout;
    if (( line = snet_getline( sn, &tv )) == NULL ) {
        fprintf( stderr, "snet_getline" );
        return( -1 );
    }
    size = atol( line );
    if ( verbose ) printf( "<<< %d\n<<< ", size );

    tv = timeout;
    /* read header to determine if file is encoded in applesingle */
    if (( rc = snet_read( sn, ( char * )&ah, AS_HEADERLEN, &tv ))
		!= AS_HEADERLEN ) {
	perror( "snet_read" ); 
	return( -1 );
    }

    if ( memcmp( &as_header, &ah, sizeof( struct as_header )) != 0 ) {
	fprintf( stderr, "%s is not a radmind AppleSingle file.\n", path );
	return( -1 );
    }
    if ( cksum ) SHA1_Update( &sha_ctx, ( char * )&ah, (size_t)rc );
    if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }
    size -= rc;

    /* read header entries */
    tv = timeout;
    if (( rc = snet_read( sn, ( char * )&ae_ents,
		( 3 * sizeof( struct as_entry )), &tv ))
		!= ( 3 * sizeof( struct as_entry ))) {
	perror( "snet_read" );
	return( -1 );
    }
    if ( cksum ) SHA1_Update( &sha_ctx, ( char * )&ae_ents, (size_t)rc );
    if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }

    size -= rc;

    /* read finder info */
    tv = timeout;
    if (( rc = snet_read( sn, finfo, sizeof( finfo ), &tv ))
		!= sizeof( finfo )) {
	perror( "snet_read" );
	return( -1 );
    }
    if ( cksum ) SHA1_Update( &sha_ctx, finfo, (size_t)rc );
    if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }
    size -= rc;

    /* name temp file */
    if ( snprintf( temppath, MAXPATHLEN, "%s.radmind.%i", path,
	    getpid()) > MAXPATHLEN ) {
	fprintf( stderr, "%s.radmind.%i: too long", path, ( int )getpid());
	return( -1 );
    }

    /* make rsrc fork name */
    if ( snprintf( rsrc_path, MAXPATHLEN, "%s%s", temppath,
		_PATH_RSRCFORKSPEC ) > MAXPATHLEN ) {
	fprintf( stderr, "%s%s: path too long\n", temppath,
		_PATH_RSRCFORKSPEC );
	return ( -1 );
    }

    /* data fork must exist to write to rsrc fork */        
    if (( dfd = open( temppath, O_CREAT | O_EXCL | O_WRONLY, 0666 )) < 0 ) {
	goto error1;
    }

    if (( rfd = open( rsrc_path, O_WRONLY, 0 )) < 0 ) {
        goto error2;
    };  

    for ( rc = 0, rsize = ae_ents[ AS_RFE ].ae_length; rsize > 0;
		rsize -= rc ) {
	tv = timeout;
	if (( rc = snet_read( sn, buf, ( int )MIN( sizeof( buf ), rsize ),
		&tv )) <= 0 ) {
	    perror( "snet_read" );
	    goto error2;
	}

	if (( write( rfd, buf, ( unsigned int )rc )) != rc ) {
	    perror( "rfd write" );
	    goto error2;
	}
	if ( cksum ) SHA1_Update( &sha_ctx, buf, (size_t)rc );
	if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }
    }

    size -= ae_ents[ AS_RFE ].ae_length;
    
    if ( close( rfd ) < 0 ) {
	perror( "close rfd" );
	exit( 1 );
    }

    /* write data fork to file */
    for ( rc = 0; size > 0; size -= rc ) {
    	tv = timeout;
    	if (( rc = snet_read( sn, buf, (int)MIN( sizeof( buf ), size ),
		&tv )) <= 0 ) {
	    perror( "snet_read" );
	    goto error2;
	}

	if ( write( dfd, buf, ( unsigned int )rc ) != rc ) {
	    perror( "dfd write" );
	    goto error2;
	}

	if ( cksum ) SHA1_Update( &sha_ctx, buf, (size_t)rc );
	if ( dodots ) { putc( '.', stdout ); fflush( stdout); }
    }

    if ( close( dfd ) < 0 ) {
	perror( "close dfd" );
	exit( 1 );
    }
    if ( verbose ) printf( "\n" );

    /* set finder info for newly decoded applefile */
    if ( setattrlist( temppath, &alist, finfo, sizeof( finfo ),
		FSOPT_NOFOLLOW ) != 0 ) {
	goto error3;
    }

    tv = timeout;
    if (( line = snet_getline( sn, &tv )) == NULL ) {
        fprintf( stderr, "snet_getline" );
        return( -1 );
    }
    if ( strcmp( line, "." ) != 0 ) {
        fprintf( stderr, "%s", line );
        return( -1 );
    }
    if ( verbose ) printf( "<<< .\n" );

    if ( cksum ) {
        SHA1_Final( md, &sha_ctx );
        base64_e( md, sizeof( md ), mde );
        if ( strcmp( cksumval, mde ) != 0 ) {
            fprintf( stderr, "checksum failed: %s\n", path );
            return( -1 );
        }
    }

    return( 0 );

error1:
    if ( close( dfd ) < 0 ) {
	perror( temppath );
	exit( 1 );
    }

    return( -1 );
error2:
    if ( close ( dfd ) < 0 ) {
	perror( temppath );
	exit( 1 );
    }

    if ( close( rfd ) < 0 ) {
	perror( rsrc_path );
	exit( 1 );
    }

    return( -1 );
error3:
    fprintf( stderr, "Couldn't set finder info for %s.\n", temppath );
    return( -1 );
}
#else !__APPLE__
#include <sys/time.h>
#include <snet.h>
#include "applefile.h"
    int
retr_applefile( SNET *sn, char *pathdesc, char *path, char *cksumval,
    char *temppath, int linenum )
{
    return( -1 );
}
#endif __APPLE__
