/*
 * Copyright (c) 2003 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#ifdef __APPLE__
#include <sys/paths.h>
#endif /* __APPLE__ */
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include <openssl/evp.h>

#include "openssl_compat.h" // Compatibility shims for OpenSSL < 1.1.0
#include "applefile.h"
#include "cksum.h"
#include "base64.h"

/*
 * do_cksum calculates the checksum for PATH and returns it base64 encoded
 * in cksum_b64 which must be of size SZ_BASE64_E( EVP_MAX_MD_SIZE ).
 *
 * return values:
 *	< 0	system error: errno set, no message given
 *	>= 0	number of bytes check summed
 */

    off_t 
do_fcksum( int fd, char *cksum_b64 )
{
    unsigned int	md_len;
    ssize_t		rr;
    off_t		size = 0;
    unsigned char	buf[ 8192 ];
    extern EVP_MD	*md;
    EVP_MD_CTX		*mdctx = EVP_MD_CTX_new();
    unsigned char	md_value[ SZ_BASE64_D( SZ_BASE64_E( EVP_MAX_MD_SIZE ) ) ];

    EVP_DigestInit( mdctx, md );

    while (( rr = read( fd, buf, sizeof( buf ))) > 0 ) {
	size += rr;
	EVP_DigestUpdate( mdctx, buf, (unsigned int)rr );
    }
    if ( rr < 0 ) {
	return( -1 );
    }

    EVP_DigestFinal( mdctx, md_value, &md_len );
    base64_e( md_value, md_len, cksum_b64 );
    EVP_MD_CTX_free( mdctx );

    return( size );
}

    off_t
do_cksum( char *path, char *cksum_b64 )
{
    int			fd;
    off_t		size = 0;

    if (( fd = open( path, O_RDONLY, 0 )) < 0 ) {
	return( -1 );
    }

    size = do_fcksum( fd, cksum_b64 );

    if ( close( fd ) != 0 ) {
	return( -1 );
    }

    return( size );
}

#ifdef __APPLE__

/*
 * do_acksum calculates the checksum for the encoded apple single file of PATH
 * and returns it base64 encoded in cksum_b64 which must be of size
 * SZ_BASE64_E( EVP_MAX_MD_SIZE ). 
 *
 * return values:
 *	>= 0	number of bytes check summed
 * 	< 0 	system error: errno set, no message given
 *
 * do_acksum should only be called on native HFS+ system.
 */

    off_t 
do_acksum( char *path, char *cksum_b64, struct applefileinfo *afinfo )
{
    int		    	    	dfd, rfd, rc;
    char			buf[ 8192 ], rsrc_path[ MAXPATHLEN ];
    off_t			size = 0;
    extern struct as_header	as_header;
    struct as_entry		as_entries_endian[ 3 ];
    unsigned int		md_len;
    extern EVP_MD		*md;
    EVP_MD_CTX          	*mdctx = EVP_MD_CTX_new();
    unsigned char		md_value[ SZ_BASE64_D( SZ_BASE64_E( EVP_MAX_MD_SIZE ) ) ];

    EVP_DigestInit( mdctx, md );

    /* checksum applesingle header */
    EVP_DigestUpdate( mdctx, (char *)&as_header, AS_HEADERLEN );
    size += (size_t)AS_HEADERLEN;

    /* endian handling, sum big-endian header entries */
    memcpy( &as_entries_endian, &afinfo->as_ents,
		( 3 * sizeof( struct as_entry )));
    as_entry_netswap( &as_entries_endian[ AS_FIE ] );
    as_entry_netswap( &as_entries_endian[ AS_RFE ] );
    as_entry_netswap( &as_entries_endian[ AS_DFE ] );

    /* checksum header entries */
    EVP_DigestUpdate( mdctx, (char *)&as_entries_endian,
		(unsigned int)( 3 * sizeof( struct as_entry )));
    size += sizeof( 3 * sizeof( struct as_entry ));

    /* checksum finder info data */
    EVP_DigestUpdate( mdctx, afinfo->ai.ai_data, FINFOLEN );
    size += FINFOLEN;

    /* checksum rsrc fork data */
    if ( afinfo->as_ents[ AS_RFE ].ae_length > 0 ) {
        if ( snprintf( rsrc_path, MAXPATHLEN, "%s%s",
		path, _PATH_RSRCFORKSPEC ) >= MAXPATHLEN ) {
            errno = ENAMETOOLONG;
            return( -1 );
        }

	if (( rfd = open( rsrc_path, O_RDONLY )) < 0 ) {
	    return( -1 );
	}
	while (( rc = read( rfd, buf, sizeof( buf ))) > 0 ) {
	    EVP_DigestUpdate( mdctx, buf, (unsigned int)rc );
	    size += (size_t)rc;
	}
	if ( close( rfd ) < 0 ) {
	    return( -1 );
	}
	if ( rc < 0 ) {
	    return( -1 );
	}
    }

    if (( dfd = open( path, O_RDONLY, 0 )) < 0 ) {
	return( -1 );
    }
    /* checksum data fork */
    while (( rc = read( dfd, buf, sizeof( buf ))) > 0 ) {
	EVP_DigestUpdate( mdctx, buf, (unsigned int)rc );
	size += (size_t)rc;
    }
    if ( rc < 0 ) {
	return( -1 );
    }
    if ( close( dfd ) < 0 ) {
	return( -1 );
    }

    EVP_DigestFinal( mdctx, md_value, &md_len );
    base64_e( ( unsigned char* ) md_value, md_len, cksum_b64 );
    EVP_MD_CTX_free( mdctx );

    return( size );
}
#else /* __APPLE__ */

/*
 * stub fuction for non-hfs+ machines.
 *
 * return values:
 * 	-1 	system error: non hfs+ system
 */

    off_t 
do_acksum( char *path, char *cksum_b64, struct applefileinfo *afino )
{
    errno = EOPNOTSUPP;
    return( -1 );
}
#endif /* __APPLE__ */
