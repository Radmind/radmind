#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#ifdef __APPLE__
#include <sys/paths.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#endif __APPLE__
#include <fcntl.h>
#include <unistd.h>
#include <snet.h>

#include <sha.h>

#include "applefile.h"
#include "chksum.h"
#include "base64.h"

extern struct as_header as_header;

    int 
do_chksum( char *path, char *chksum_b64 )
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
    strcpy( chksum_b64, mde );

    if ( close( fd ) != 0 ) {
	return( -1 );
    }

    return( 0 );
}

#ifdef __APPLE__
int
do_achksum( char *path, char *chksum_b64 )
{
    int			afd;
    int		    	rfd, r_cc, d_cc, d_size, r_size, err, has_rsrc = 0;
    unsigned char	md[ SHA_DIGEST_LENGTH ];
    unsigned char	mde[ SZ_BASE64_E( sizeof( md )) ];
    char		data_buf[ 8192 ];
    char		finfo_buf[ 32 ] = { 0 };
    char	    	rsrc_path[ MAXPATHLEN ];
    const char	    	*rsrc_suffix = _PATH_RSRCFORKSPEC; /* sys/paths.h */
    struct as_entry	as_entry_finfo = { ASEID_FINFO, 62, 32 };
    struct as_entry	as_entry_rfork = { ASEID_RFORK, 94, 0 };
    struct as_entry	as_entry_dfork = { ASEID_DFORK, 0, 0 };
    struct stat		r_stp;	    /* for rsrc fork */
    struct stat		d_stp;	    /* for data fork */
    SHA_CTX		sha_ctx;

    SHA1_Init( &sha_ctx );

    if (( afd = open( path, O_RDONLY, 0 )) < 0 ) {
	return( -1 );
    }

    if ( snprintf( rsrc_path, MAXPATHLEN, "%s%s", path, rsrc_suffix ) 
		> MAXPATHLEN ) {
	fprintf( stderr, "%s%s: path too long\n", path, rsrc_suffix );
	return( -1 );
    }
		
    if ( lstat( path, &d_stp ) != 0 ) {
	perror( path );
	return( -1 );
    }

    if 	( lstat( rsrc_path, &r_stp ) != 0 ) {
	/* if there's no rsrc fork, but there is finder info,
	 * assume zero length rsrc fork.
	 */
	if ( errno == ENOENT ) {
	    r_size = 0;
    	} else {
	    perror( rsrc_path );
	    return( 1 );
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
	    return( 1 );
	}
    }

    /* rsrc fork header entry */
    as_entry_rfork.ae_length = r_size;

    /* data fork header entry */
    as_entry_dfork.ae_offset = 
	( as_entry_rfork.ae_offset + as_entry_rfork.ae_length );
    as_entry_dfork.ae_length = d_size;

    /* checksum  applesingle header to server */
    SHA1_Update( &sha_ctx, &as_header, (size_t)AS_HEADERLEN );

    /* checksum header entry for finder info */
    SHA1_Update( &sha_ctx, &as_entry_finfo, (size_t)sizeof( as_entry_finfo ));

    /* checksum header entry for rsrc fork */
    SHA1_Update( &sha_ctx, &as_entry_rfork, (size_t)sizeof( as_entry_rfork ));

    /* checksum header entry for data fork */
    SHA1_Update( &sha_ctx, &as_entry_dfork, (size_t)sizeof( as_entry_dfork ));

    err = chk_for_finfo( path, finfo_buf );
    if ( err ) {
	fprintf( stderr, "Non-hfs system\n" );
	return( -1 );
    }

    /* checksum finder info data to server */
    SHA1_Update( &sha_ctx, &finfo_buf, (size_t)sizeof( finfo_buf ));

    /* checksum rsrc fork data to server */
    if ( has_rsrc ) {
	while (( r_cc = read( rfd, data_buf, sizeof( data_buf ))) > 0 ) {
	    SHA1_Update( &sha_ctx, &data_buf, (size_t)r_cc );
	}
    }

    if ( r_cc < 0 ) {
	return( -1 );
    }

    /* checksum data fork to server */
    while (( d_cc = read( afd, data_buf, sizeof( data_buf ))) > 0 ) {
	SHA1_Update( &sha_ctx, &data_buf, (size_t)d_cc );
    }

    if ( d_cc < 0 ) {
	return( -1 );
    }

    if ( has_rsrc ) {
	if ( close( rfd ) < 0 ) {
	    perror( "close rfd" );
	    return( -1 );
	}
    }

    if ( close( afd ) < 0 ) {
	perror( "close afd" );
	return( -1 );
    }

    /* free all the alloc'd memory */
   
    SHA1_Final( md, &sha_ctx );

    base64_e( md, sizeof( md ), mde );
    strcpy( chksum_b64, mde );

    return( 0 );
}
#endif __APPLE__
