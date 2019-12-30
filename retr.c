/*
 * Copyright (c) 2003 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/time.h>
#ifdef __APPLE__
#include <sys/attr.h>
#include <sys/paths.h>
#endif /* __APPLE__ */
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

#include <openssl/evp.h>

#ifdef HAVE_ZLIB
#include <zlib.h>
#endif /* HAVE_ZLIB */

#include <snet.h>

#include "openssl_compat.h" // Compatibility shims for OpenSSL < 1.1.0
#include "applefile.h"
#include "connect.h"
#include "cksum.h"
#include "base64.h"
#include "code.h"
#include "largefile.h"
#include "progress.h"
#include "mkprefix.h"

extern void            (*logger)( char * );
extern struct timeval  	timeout;
extern int 		linenum;
extern int		verbose;
extern int		showprogress;
extern int		dodots;
extern int		cksum;
extern int		errno;
extern int		create_prefix;
extern SSL_CTX  	*ctx;

/*
 * Download requests path from sn and writes it to disk.  The path to
 * this new file is returned via temppath which must be 2 * MAXPATHLEN.
 * 
 * Return Value:
 *	-1 - error, do not call closesn
 *	 0 - OKAY
 *	 1 - error, call closesn
 */

    int 
retr( SNET *sn, char *pathdesc, char *path, char *temppath, mode_t tempmode,
    off_t transize, char *trancksum )
{
    struct timeval	tv;
    char		*line;
    int			fd;
    unsigned int	md_len;
    int			returnval = -1;
    off_t		size = 0;
    char		buf[ 8192 ]; 
    ssize_t		rr;
    extern EVP_MD	*md;
    EVP_MD_CTX		*mdctx = EVP_MD_CTX_new();
    unsigned char	md_value[ SZ_BASE64_D( SZ_BASE64_E( EVP_MAX_MD_SIZE ) ) ];
    char		cksum_b64[ SZ_BASE64_E( EVP_MAX_MD_SIZE ) ];

    if ( cksum ) {
	if ( strcmp( trancksum, "-" ) == 0 ) {
	    fprintf( stderr, "line %d: No checksum\n", linenum);
	    fprintf( stderr, "%s\n", pathdesc );
	    return( 1 );
	}
	EVP_DigestInit( mdctx, md );
    }

    if ( verbose ) printf( ">>> RETR %s\n", pathdesc );
    if ( snet_writef( sn, "RETR %s\n", pathdesc ) < 0 ) {
	fprintf( stderr, "retrieve %s failed: 1-%s\n", pathdesc,
	    strerror( errno ));
	return( -1 );
    }

    tv = timeout;
    if (( line = snet_getline_multi( sn, logger, &tv )) == NULL ) {
	fprintf( stderr, "retrieve %s failed: 2-%s\n", pathdesc,
	    strerror( errno ));
	return( -1 );
    }

    if ( *line != '2' ) {
	fprintf( stderr, "%s\n", line );
	return( 1 );
    }

    /* Get file size from server */
    tv = timeout;
    if (( line = snet_getline( sn, &tv )) == NULL ) {
	fprintf( stderr, "retrieve %s failed: 3-%s\n", pathdesc,
	    strerror( errno ));
	return( -1 );
    }
    size = strtoofft( line, NULL, 10 );
    if ( verbose ) printf( "<<< %" PRIofft "d\n", size );
    if ( transize >= 0 && size != transize ) {
	fprintf( stderr, "line %d: size in transcript does not match size "
	    "from server\n", linenum );
	fprintf( stderr, "%s\n", pathdesc );
	return( -1 );
    }

    /*Create temp file name*/
    if ( snprintf( temppath, MAXPATHLEN, "%s.radmind.%i",
	    path, getpid()) >= MAXPATHLEN ) {
	fprintf( stderr, "%s.radmind.%i: too long", path,
		(int)getpid());
	return( -1 );
    }
    /* Open file */
    if (( fd = open( temppath, O_WRONLY | O_CREAT, tempmode )) < 0 ) {
	if ( create_prefix && errno == ENOENT ) {
	    errno = 0;
	    if ( mkprefix( temppath ) != 0 ) {
		perror( temppath );
		return( -1 );
	    }
	    if (( fd = open( temppath, O_WRONLY | O_CREAT, tempmode )) < 0 ) {
		perror( temppath );
		return( -1 );
	    }
	} else {
	    perror( temppath );
	    return( -1 );
	}
    }

    if ( verbose ) printf( "<<< " );

    /* Get file from server */
    while ( size > 0 ) {
	tv = timeout;
	if (( rr = snet_read( sn, buf, MIN( sizeof( buf ), size ),
		&tv )) <= 0 ) {
	    fprintf( stderr, "retrieve %s failed: 4-%s\n", pathdesc,
		strerror( errno ));
	    returnval = -1;
	    goto error2;
	}
	if ( write( fd, buf, (size_t)rr ) != rr ) {
	    perror( temppath );
	    returnval = -1;
	    goto error2;
	}
	if ( cksum ) {
	    EVP_DigestUpdate( mdctx, buf, (unsigned int)rr );
	}
	if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }
	size -= rr;
	if ( showprogress ) {
	    progressupdate( rr, path );
	}
    }
    if ( close( fd ) != 0 ) {
	perror( path );
	returnval = -1;
	goto error1;
    }
    if ( verbose ) printf( "\n" );

    tv = timeout;
    if (( line = snet_getline( sn, &tv )) == NULL ) {
	fprintf( stderr, "retrieve %s failed: 5-%s\n", pathdesc,
	    strerror( errno ));
	returnval = -1;
	goto error1;
    }
    if ( strcmp( line, "." ) != 0 ) {
	fprintf( stderr, "%s", line );
	fprintf( stderr, "%s\n", pathdesc );
	returnval = -1;
	goto error1;
    }
    if ( verbose ) printf( "<<< .\n" );

    /* cksum file */
    if ( cksum ) {
	EVP_DigestFinal( mdctx, md_value, &md_len );
	base64_e( md_value, md_len, cksum_b64 );
	EVP_MD_CTX_free(mdctx);
	if ( strcmp( trancksum, cksum_b64 ) != 0 ) {
	    fprintf( stderr, "line %d: checksum in transcript does not match "
		"checksum from server\n", linenum );
	    fprintf( stderr, "%s\n", pathdesc );
	    returnval = 1;
	    goto error1;
	}
    }

    return( 0 );

error2:
    close( fd );
error1:
    unlink( temppath );
    return( returnval );
}

#ifdef __APPLE__

/*
 * Return Value:
 *	-1 - error, do not call closesn
 *	 0 - OKAY
 *	 1 - error, call closesn
 */

    int
retr_applefile( SNET *sn, char *pathdesc, char *path, char *temppath,
    mode_t tempmode, off_t transize, char *trancksum )
{
    int				dfd, rfd;
    unsigned int		md_len;
    int				returnval = -1;
    off_t			size;
    size_t			rsize;
    ssize_t			rc;
    char			finfo[ FINFOLEN ];
    char			buf[ 8192 ];
    char			rsrc_path[ MAXPATHLEN ];
    char			*line;
    struct as_header		ah;
    extern struct as_header	as_header;
    extern struct attrlist	setalist;
    struct as_entry		ae_ents[ 3 ]; 
    struct timeval		tv;
    extern EVP_MD       	*md;
    EVP_MD_CTX   	       	*mdctx;
    unsigned char       	md_value[ SZ_BASE64_D( SZ_BASE64_E( EVP_MAX_MD_SIZE ) ) ];
    char		       	cksum_b64[ SZ_BASE64_E( EVP_MAX_MD_SIZE ) ];

    if ( cksum ) {
        if ( strcmp( trancksum, "-" ) == 0 ) {
	    fprintf( stderr, "line %d: No checksum\n", linenum);
	    fprintf( stderr, "%s\n", pathdesc );
            return( 1 );
        }
        EVP_DigestInit( mdctx, md );
    }

    if ( verbose ) printf( ">>> RETR %s\n", pathdesc );
    if ( snet_writef( sn, "RETR %s\n", pathdesc ) < 0 ) {
	fprintf( stderr, "retrieve applefile %s failed: 1-%s\n", pathdesc,
	    strerror( errno ));
	return( -1 );
    }

    tv = timeout;
    if (( line = snet_getline_multi( sn, logger, &tv )) == NULL ) {
	fprintf( stderr, "retrieve applefile %s failed: 2-%s\n", pathdesc,
	    strerror( errno ));
	return( -1 );
    }

    if ( *line != '2' ) {
        fprintf( stderr, "%s\n", line );
        return( 1 );
    }

    /* Get file size from server */
    tv = timeout;
    if (( line = snet_getline( sn, &tv )) == NULL ) {
	fprintf( stderr, "retrieve applefile %s failed: 3-%s\n", pathdesc,
	    strerror( errno ));
	return( -1 );
    }
    size = strtoofft( line, NULL, 10 );
    if ( verbose ) printf( "<<< %" PRIofft "d\n", size );
    if ( transize >= 0 && size != transize ) {
	fprintf( stderr, "line %d: size in transcript does not match size"
	    "from server\n", linenum );
	fprintf( stderr, "%s\n", pathdesc );
	return( -1 );
    }  
    if ( size < ( AS_HEADERLEN + ( 3 * sizeof( struct as_entry )) +
	    FINFOLEN )) {
	fprintf( stderr,
	    "retrieve applefile %s failed: AppleSingle-encoded file too "
	    "short\n", path );
	return( -1 );
    }

    /* read header to determine if file is encoded in applesingle */
    tv = timeout;
    if (( rc = snet_read( sn, ( char * )&ah, AS_HEADERLEN, &tv )) <= 0 ) {
	fprintf( stderr, "retrieve applefile %s failed: 4-%s\n", pathdesc,
	    strerror( errno ));
	return( -1 );
    }
    if (( rc != AS_HEADERLEN ) ||
	    ( memcmp( &as_header, &ah, AS_HEADERLEN ) != 0 )) {
	fprintf( stderr,
	    "retrieve applefile %s failed: corrupt AppleSingle-encoded file\n",
	    path );
	return( -1 );
    }
    if ( cksum ) {
	EVP_DigestUpdate( mdctx, (char *)&ah, (unsigned int)rc );
    }

    /* name temp file */
    if ( snprintf( temppath, MAXPATHLEN, "%s.radmind.%i", path,
	    getpid()) >= MAXPATHLEN ) {
	fprintf( stderr, "%s.radmind.%i: too long", path, ( int )getpid());
	return( -1 );
    }

    /* data fork must exist to write to rsrc fork */        
    /* Open here so messages from mkprefix don't verbose dots */
    if (( dfd = open( temppath, O_CREAT | O_EXCL | O_WRONLY, tempmode )) < 0 ) {
	if ( create_prefix && errno == ENOENT ) {
	    errno = 0;
	    if ( mkprefix( temppath ) != 0 ) {
		perror( temppath );
		return( -1 );
	    }
	    if (( dfd = open( temppath, O_CREAT | O_EXCL | O_WRONLY,
		    tempmode )) < 0 ) {
		perror( temppath );
		return( -1 );
	    }
	} else {
	    perror( temppath );
	    return( -1 );
	}
    }

    if ( verbose ) printf( "<<< " );
    if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }
    size -= rc;
    if ( showprogress ) {
	progressupdate( rc, path );
    }

    /* read header entries */
    tv = timeout;
    if (( rc = snet_read( sn, ( char * )&ae_ents,
	    ( 3 * sizeof( struct as_entry )), &tv )) <= 0 ) {
	fprintf( stderr, "retrieve applefile %s failed: 5-%s\n", pathdesc,
	    strerror( errno ));
	returnval = -1;
	goto error2;
    }
    if ( rc != ( 3 * sizeof( struct as_entry ))) {
	fprintf( stderr,
	    "retrieve applefile %s failed: corrupt AppleSingle-encoded file\n",
	    path );
	returnval = -1;
	goto error2;
    }

    /* Should we check for valid ae_ents here? YES! */

    if ( cksum ) {
	EVP_DigestUpdate( mdctx, (char *)&ae_ents, (unsigned int)rc );
    }
    if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }

    size -= rc;
    if ( showprogress ) {
	progressupdate( rc, path );
    }

    /* read finder info */
    tv = timeout;
    if (( rc = snet_read( sn, finfo, FINFOLEN, &tv )) <= 0 ) {
	fprintf( stderr, "retrieve applefile %s failed: 6-%s\n", pathdesc,
	    strerror( errno ));
	returnval = -1;
	goto error2;
    }
    if ( rc != FINFOLEN ) {
	fprintf( stderr,
	    "retrieve applefile %s failed: corrupt AppleSingle-encoded file\n",
	    path );
	returnval = -1;
	goto error2;
    }
    if ( cksum ) {
	EVP_DigestUpdate( mdctx, finfo, (unsigned int)rc );
    }
    if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }
    size -= rc;
    if ( showprogress ) {
	progressupdate( rc, path );
    }

    /*
     * endian handling: swap bytes to architecture
     * native from AppleSingle big-endian.
     *
     * This doesn't affect the checksum, since we
     * already summed the header entries above.
     */
    as_entry_hostswap( &ae_ents[ AS_RFE ] );

    if ( ae_ents[ AS_RFE ].ae_length > 0 ) {
	/* make rsrc fork name */
	if ( snprintf( rsrc_path, MAXPATHLEN, "%s%s", temppath,
		_PATH_RSRCFORKSPEC ) >= MAXPATHLEN ) {
	    fprintf( stderr, "%s%s: path too long\n", temppath,
		_PATH_RSRCFORKSPEC );
	    returnval = -1;
	    goto error2;
	}

	/* No need to mkprefix as dfd is already present */
	if (( rfd = open( rsrc_path, O_WRONLY, 0 )) < 0 ) {
	    perror( rsrc_path );
	    returnval = -1;
	    goto error2;
	}

	for ( rsize = ae_ents[ AS_RFE ].ae_length;
					rsize > 0; rsize -= rc ) {
	    tv = timeout;
	    if (( rc = snet_read( sn, buf, ( int )MIN( sizeof( buf ), rsize ),
		    &tv )) <= 0 ) {
		fprintf( stderr, "retrieve applefile %s failed: 7-%s\n",
		    pathdesc, strerror( errno ));
		returnval = -1;
		goto error3;
	    }
	    if (( write( rfd, buf, ( unsigned int )rc )) != rc ) {
		perror( rsrc_path );
		returnval = -1;
		goto error3;
	    }
	    if ( cksum ) {
		EVP_DigestUpdate( mdctx, buf, (unsigned int)rc );
	    }
	    if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }
	    if ( showprogress ) {
		progressupdate( rc, path );
	    }
	}

	size -= ae_ents[ AS_RFE ].ae_length;
    
	if ( close( rfd ) < 0 ) {
	    perror( rsrc_path );
	    goto error2;
	}
    }

    /* write data fork to file */
    for ( rc = 0; size > 0; size -= rc ) {
    	tv = timeout;
    	if (( rc = snet_read( sn, buf, MIN( sizeof( buf ), size ),
		&tv )) <= 0 ) {
	    fprintf( stderr, "retrieve applefile %s failed: 8-%s\n", pathdesc,
		strerror( errno ));
	    returnval = -1;
	    goto error2;
	}

	if ( write( dfd, buf, ( unsigned int )rc ) != rc ) {
	    perror( temppath );
	    returnval = -1;
	    goto error2;
	}

	if ( cksum ) {
	    EVP_DigestUpdate( mdctx, buf, (unsigned int)rc );
	}
	if ( dodots ) { putc( '.', stdout ); fflush( stdout); }
	if ( showprogress ) {
	    progressupdate( rc, path );
	}
    }

    if ( close( dfd ) < 0 ) {
	perror( temppath );
	returnval = -1;
	goto error1;
    }
    if ( verbose ) printf( "\n" );

    /* set finder info for newly decoded applefile */
    if ( setattrlist( temppath, &setalist, finfo, FINFOLEN,
	    FSOPT_NOFOLLOW ) != 0 ) {
	fprintf( stderr,
	    "retrieve applefile %s failed: Could not set attributes\n",
	    pathdesc );
	returnval = -1;
	goto error1;
    }

    tv = timeout;
    if (( line = snet_getline( sn, &tv )) == NULL ) {
	fprintf( stderr, "retrieve applefile %s failed: 9-%s\n", pathdesc,
	    strerror( errno ));
	returnval = -1;
	goto error1;
    }
    if ( strcmp( line, "." ) != 0 ) {
        fprintf( stderr, "%s", line );
	fprintf( stderr, "%s\n", pathdesc );
	returnval = -1;
	goto error1;
    }
    if ( verbose ) printf( "<<< .\n" );

    if ( cksum ) {
	EVP_DigestFinal( mdctx, md_value, &md_len );
	base64_e(( char*)&md_value, md_len, cksum_b64 );
        EVP_MD_CTX_free(mdctx);
        if ( strcmp( trancksum, cksum_b64 ) != 0 ) {
	    fprintf( stderr, "line %d: checksum in transcript does not match "
		"checksum from server\n", linenum );
	fprintf( stderr, "%s\n", pathdesc );
            returnval = 1;
	    goto error1;
        }
    }

    return( 0 );

error3:
    close( rfd );
error2:
    close( dfd );
error1:
    unlink( temppath );
    return( returnval );
}

#else /* !__APPLE__ */

    int
retr_applefile( SNET *sn, char *pathdesc, char *path, char *temppath,
    mode_t tempmode, off_t transize, char *trancksum )
{
    errno = EINVAL;
    return( -1 );
}

#endif /* __APPLE__ */
