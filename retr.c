/*
 * Copyright (c) 2002 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
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

#ifdef TLS
#include <openssl/ssl.h>
#include <openssl/err.h>

extern SSL_CTX  *ctx;
#endif TLS

#include <openssl/evp.h>
#include <snet.h>

#include "applefile.h"
#include "connect.h"
#include "cksum.h"
#include "base64.h"
#include "code.h"

#ifdef sun
#define MIN(X, Y) ((X) < (Y) ? (X) : (Y))
#endif /* sun */

extern void            (*logger)( char * );
extern struct timeval  	timeout;
extern int 		linenum;
extern int		verbose;
extern int		dodots;
extern int		cksum;
extern int		errno;

/*
 * Download requests path from sn and writes it to disk.  The path to
 * this new file is returned via temppath which must be 2 * MAXPATHLEN.
 * 
 * Return Value:
 *	-1 - erorr, do not call closesn
 *	 0 - OKAY
 *	 1 - error, call closesn
 */

    int 
retr( SNET *sn, char *pathdesc, char *path, char *temppath, ssize_t transize,
    char *trancksum )
{
    struct timeval      tv;
    char 		*line;
    int			fd, md_len;
    int			returnval = -1;
    size_t              size = 0;
    unsigned char       buf[ 8192 ]; 
    ssize_t             rr;
    extern EVP_MD       *md;
    EVP_MD_CTX          mdctx;
    unsigned char       md_value[ EVP_MAX_MD_SIZE ];
    unsigned char       cksum_b64[ SZ_BASE64_E( EVP_MAX_MD_SIZE ) ];

    if ( cksum ) {
	if ( strcmp( trancksum, "-" ) == 0 ) {
	    fprintf( stderr, "line %d: No checksum\n", linenum);
	    return( 1 );
	}
	EVP_DigestInit( &mdctx, md );
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
    size = atoi( line );
    if ( verbose ) printf( "<<< %ld\n", (long)size );
    if ( transize >= 0 && size != transize ) {
	fprintf( stderr, "line %d: size in transcript does not match size "
	    "from server\n", linenum );
	return( -1 );
    }
    if ( verbose ) printf( "<<< " );

    /*Create temp file name*/
    if ( snprintf( temppath, MAXPATHLEN, "%s.radmind.%i",
	    path, getpid()) >= MAXPATHLEN ) {
	fprintf( stderr, "%s.radmind.%i: too long", path,
		(int)getpid());
	return( -1 );
    }
    /* Open file */
    if (( fd = open( temppath, O_RDWR | O_CREAT | O_EXCL, 0600 )) < 0 ) {
	perror( temppath );
	return( -1 );
    }

    /* Get file from server */
    while ( size > 0 ) {
	tv = timeout;
	if (( rr = snet_read( sn, buf, (int)MIN( sizeof( buf ), size ),
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
	    EVP_DigestUpdate( &mdctx, buf, (unsigned int)rr );
	}
	if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }
	size -= rr;
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
	returnval = -1;
	goto error1;
    }
    if ( verbose ) printf( "<<< .\n" );

    /* cksum file */
    if ( cksum ) {
	EVP_DigestFinal( &mdctx, md_value, &md_len );
	base64_e(( char*)&md_value, md_len, cksum_b64 );
	if ( strcmp( trancksum, cksum_b64 ) != 0 ) {
	    fprintf( stderr, "line %d: checksum in transcript does not match "
		"checksum from server\n", linenum );
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
 *	-1 - erorr, do not call closesn
 *	 0 - OKAY
 *	 1 - error, call closesn
 */

    int
retr_applefile( SNET *sn, char *pathdesc, char *path, char *temppath,
    ssize_t transize, char *trancksum )
{
    int				dfd, rfd, rc, md_len;
    int				returnval = -1;
    size_t			size, rsize;
    char			finfo[ 32 ];
    char			buf[ 8192 ];
    char			rsrc_path[ MAXPATHLEN ];
    char			*line;
    struct as_header		ah;
    extern struct as_header	as_header;
    extern struct attrlist	alist;
    struct as_entry		ae_ents[ 3 ]; 
    struct timeval		tv;
    extern EVP_MD       	*md;
    EVP_MD_CTX   	       	mdctx;
    unsigned char       	md_value[ EVP_MAX_MD_SIZE ];
    unsigned char       	cksum_b64[ SZ_BASE64_E( EVP_MAX_MD_SIZE ) ];

    if ( cksum ) {
        if ( strcmp( trancksum, "-" ) == 0 ) {
	    fprintf( stderr, "line %d: No checksum\n", linenum);
            return( 1 );
        }
        EVP_DigestInit( &mdctx, md );
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
    size = atol( line );
    if ( verbose ) printf( "<<< %ld\n", size );
    if ( transize >= 0 && size != transize ) {
	fprintf( stderr, "line %d: size in transcript does not match size"
	    "from server\n", linenum );
	return( -1 );
    }  
    if ( verbose ) printf( "<<< " );
    if ( size < ( AS_HEADERLEN + ( 3 * sizeof( struct as_entry )) +
	    sizeof( finfo ))) {
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
	EVP_DigestUpdate( &mdctx, (char *)&ah, (unsigned int)rc );
    }
    if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }
    size -= rc;

    /* read header entries */
    tv = timeout;
    if (( rc = snet_read( sn, ( char * )&ae_ents,
	    ( 3 * sizeof( struct as_entry )), &tv )) <= 0 ) {
	fprintf( stderr, "retrieve applefile %s failed: 5-%s\n", pathdesc,
	    strerror( errno ));
	return( -1 );
    }
    if ( rc != ( 3 * sizeof( struct as_entry ))) {
	fprintf( stderr,
	    "retrieve applefile %s failed: corrupt AppleSingle-encoded file\n",
	    path );
	return( -1 );
    }

    /* Should we check for valid ae_ents here? YES! */

    if ( cksum ) {
	EVP_DigestUpdate( &mdctx, (char *)&ae_ents, (unsigned int)rc );
    }
    if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }

    size -= rc;

    /* read finder info */
    tv = timeout;
    if (( rc = snet_read( sn, finfo, sizeof( finfo ), &tv )) <= 0 ) {
	fprintf( stderr, "retrieve applefile %s failed: 6-%s\n", pathdesc,
	    strerror( errno ));
	return( -1 );
    }
    if ( rc != sizeof( finfo )) {
	fprintf( stderr,
	    "retrieve applefile %s failed: corrupt AppleSingle-encoded file\n",
	    path );
	return( -1 );
    }
    if ( cksum ) {
	EVP_DigestUpdate( &mdctx, finfo, (unsigned int)rc );
    }
    if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }
    size -= rc;

    /* name temp file */
    if ( snprintf( temppath, MAXPATHLEN, "%s.radmind.%i", path,
	    getpid()) >= MAXPATHLEN ) {
	fprintf( stderr, "%s.radmind.%i: too long", path, ( int )getpid());
	return( -1 );
    }

    /* data fork must exist to write to rsrc fork */        
    if (( dfd = open( temppath, O_CREAT | O_EXCL | O_WRONLY, 0600 )) < 0 ) {
	perror( temppath );
	return( -1 );
    }

    if ( ae_ents[ AS_RFE ].ae_length > 0 ) {
	/* make rsrc fork name */
	if ( snprintf( rsrc_path, MAXPATHLEN, "%s%s", temppath,
		_PATH_RSRCFORKSPEC ) >= MAXPATHLEN ) {
	    fprintf( stderr, "%s%s: path too long\n", temppath,
		_PATH_RSRCFORKSPEC );
	    returnval = -1;
	    goto error2;
	}

	if (( rfd = open( rsrc_path, O_WRONLY, 0 )) < 0 ) {
	    perror( rsrc_path );
	    returnval = -1;
	    goto error2;
	};  

	for ( rsize = ae_ents[ AS_RFE ].ae_length; rsize > 0; rsize -= rc ) {
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
		EVP_DigestUpdate( &mdctx, buf, (unsigned int)rc );
	    }
	    if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }
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
    	if (( rc = snet_read( sn, buf, (int)MIN( sizeof( buf ), size ),
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
	    EVP_DigestUpdate( &mdctx, buf, (unsigned int)rc );
	}
	if ( dodots ) { putc( '.', stdout ); fflush( stdout); }
    }

    if ( close( dfd ) < 0 ) {
	perror( temppath );
	returnval = -1;
	goto error1;
    }
    if ( verbose ) printf( "\n" );

    /* set finder info for newly decoded applefile */
    if ( setattrlist( temppath, &alist, finfo, sizeof( finfo ),
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
	returnval = -1;
	goto error1;
    }
    if ( verbose ) printf( "<<< .\n" );

    if ( cksum ) {
	EVP_DigestFinal( &mdctx, md_value, &md_len );
	base64_e(( char*)&md_value, md_len, cksum_b64 );
        if ( strcmp( trancksum, cksum_b64 ) != 0 ) {
	    fprintf( stderr, "line %d: checksum in transcript does not match "
		"checksum from server\n", linenum );
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
    ssize_t transize, char *trancksum )
{
    errno = EINVAL;
    return( -1 );
}

#endif /* __APPLE__ */
