#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#ifdef __APPLE__
#include <sys/paths.h>
#endif __APPLE__
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <snet.h>

#include <sha.h>

#include "cksum.h"
#include "base64.h"
#ifdef __APPLE__
#include "applefile.h"
extern struct as_header as_header;
#endif __APPLE__

    int 
do_cksum( char *path, char *cksum_b64 )
{
    int			fd;
    ssize_t		rr;
    unsigned char	buf[ 8192 ];
    unsigned char	md[ SHA_DIGEST_LENGTH ];
    unsigned char	mde[ SZ_BASE64_E( sizeof( md )) ];
    SHA_CTX		sha_ctx;

    if (( fd = open( path, O_RDONLY, 0 )) < 0 ) {
	return( -1 );
    }

    SHA1_Init( &sha_ctx );

    while (( rr = read( fd, buf, sizeof( buf ))) > 0 ) {
	SHA1_Update( &sha_ctx, buf, (size_t)rr );
    }

    if ( rr < 0 ) {
	return( -1 );
    }

    SHA1_Final( md, &sha_ctx );

    base64_e( md, sizeof( md ), mde );
    strcpy( cksum_b64, mde );

    if ( close( fd ) != 0 ) {
	return( -1 );
    }

    return( 0 );
}

#ifdef __APPLE__
    int
do_acksum( char *path, char *cksum_b64, char *finfo_buf )
{
    int		    	    	afd, rfd, rc;
    unsigned char		md[ SHA_DIGEST_LENGTH ];
    unsigned char		mde[ SZ_BASE64_E( sizeof( md )) ];
    char			buf[ 8192 ];
    char	    		rsrc_path[ MAXPATHLEN ];
    static struct as_entry	ae_ents[ 3 ] = {{ ASEID_FINFO, 62, 32 },
						{ ASEID_RFORK, 94, 0 },
						{ ASEID_DFORK, 0, 0 }};
    struct stat			r_stp;	    /* for rsrc fork */
    struct stat			d_stp;	    /* for data fork */
    SHA_CTX			sha_ctx;

    /* chk for finder info first */
    if ( finfo_buf == NULL ) {
	fprintf( stderr, "Non-hfs system\n" );
	return( -1 );
    }

    SHA1_Init( &sha_ctx );

    if (( afd = open( path, O_RDONLY, 0 )) < 0 ) {
	perror( path );
	return( -1 );
    }

    if ( fstat( afd, &d_stp ) != 0 ) {
	perror( path );
	return( -1 );
    }

    if ( snprintf( rsrc_path, MAXPATHLEN, "%s%s", path, _PATH_RSRCFORKSPEC ) 
		> MAXPATHLEN ) {
	fprintf( stderr, "%s%s: path too long\n", path, _PATH_RSRCFORKSPEC );
	return( -1 );
    }

    if (( rfd = open( rsrc_path, O_RDONLY )) < 0 ) {
	perror( rsrc_path );
	return( -1 );
    }

    if 	( fstat( rfd, &r_stp ) != 0 ) {
	/* if there's no rsrc fork, but there is finder info,
	 * assume zero length rsrc fork.
	 */
	if ( errno == ENOENT ) {
	    ae_ents[ AS_RFE ].ae_length = 0;
    	} else {
	    perror( rsrc_path );
	    return( -1 );
	}
    } else {
    	ae_ents[ AS_RFE ].ae_length = ( int )r_stp.st_size;
    }

    ae_ents[ AS_DFE ].ae_offset = 
	( ae_ents[ AS_RFE ].ae_offset + ae_ents[ AS_RFE ].ae_length );
    ae_ents[ AS_DFE ].ae_length = ( int )d_stp.st_size;

    /* checksum applesingle header */
    SHA1_Update( &sha_ctx, &as_header, ( size_t )AS_HEADERLEN );
fprintf( stderr, "sizeof( ae_ents ): %d\n", sizeof( ae_ents ));
    /* checksum header entries */
    SHA1_Update( &sha_ctx, &ae_ents, sizeof( ae_ents ));

    /* checksum finder info data */
    SHA1_Update( &sha_ctx, finfo_buf, ( size_t )32 );

    /* checksum rsrc fork data */
    if ( rfd >= 0 ) {
	while (( rc = read( rfd, buf, sizeof( buf ))) > 0 ) {
	    SHA1_Update( &sha_ctx, &buf, ( size_t )rc );
	}
	if ( close( rfd ) < 0 ) {
	    perror( "close rfd" );
	    return( -1 );
	}
    }

    if ( rc < 0 ) {
	return( -1 );
    }

    /* checksum data fork */
    while (( rc = read( afd, buf, sizeof( buf ))) > 0 ) {
	SHA1_Update( &sha_ctx, &buf, ( size_t )rc );
    }

    if ( rc < 0 ) {
	return( -1 );
    }

    if ( close( afd ) < 0 ) {
	perror( "close afd" );
	return( -1 );
    }

    SHA1_Final( md, &sha_ctx );

    base64_e( md, sizeof( md ), mde );
    strcpy( cksum_b64, mde );

    return( 0 );
}
#else __APPLE__
    int
do_acksum( char *path, char *cksum_b64, char *finfo_buf )
{
    return( -1 );
}
#endif __APPLE__
