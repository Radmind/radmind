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
#include <sys/paths.h>
#include <sys/attr.h>
#endif /* __APPLE__ */
#include <sys/uio.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

#include <openssl/evp.h>
#include <snet.h>

#include "applefile.h"
#include "connect.h"
#include "cksum.h"
#include "base64.h"
#include "code.h"
#include "largefile.h"

extern struct timeval	timeout;
extern struct as_header as_header;
extern int		verbose;
extern int		quiet;
extern int		dodots;
extern int		cksum;
extern int		linenum;
extern int		force;
extern void            	(*logger)( char * );
extern SSL_CTX  	*ctx;

    int
n_stor_file( SNET *sn, char *pathdesc, char *path )
{
    struct timeval      tv;
    char                *line;

    /* tell server what it is getting */
    if ( snet_writef( sn, "%s", pathdesc ) < 0 ) {
	fprintf( stderr, "store %s failed: %s\n", pathdesc,
	    strerror( errno ));
	goto sn_error;
    }
    if ( verbose ) printf( ">>> %s", pathdesc );
    tv = timeout;
    if (( line = snet_getline_multi( sn, logger, &tv )) == NULL ) {
	if ( snet_eof( sn )) {
	    fprintf( stderr, "store %s failed: Connection closed\n",
		pathdesc );
	} else {
	    fprintf( stderr, "store %s failed: %s\n", pathdesc,
		strerror( errno ));
	}
	goto sn_error;
    }
    if ( *line != '3' ) {
	/* Error from server - transaction aborted */
        fprintf( stderr, "%s\n", line );
        return( -1 );
    }

    /* tell server how much data to expect and send '.' */
    if ( snet_writef( sn, "0\r\n.\r\n" ) < 0 ) {
	fprintf( stderr, "store %s failed: %s\n", pathdesc,
	    strerror( errno ));
	goto sn_error;
    }
    if ( verbose ) fputs( ">>> 0\n\n>>> .\n", stdout );

    tv = timeout;
    if (( line = snet_getline_multi( sn, logger, &tv )) == NULL ) {
	if ( snet_eof( sn )) {
	    fprintf( stderr, "store %s failed: Connection closed\n",
		pathdesc );
	} else {
	    fprintf( stderr, "store %s failed: %s\n", pathdesc,
		strerror( errno ));
	}
	goto sn_error;
    }
    if ( *line != '2' ) {
	/* Error from server - transaction aborted */
        fprintf( stderr, "%s\n", line );
        return( -1 );
    }

    /* Done with server */

    if ( !quiet && !verbose ) {
        printf( "%s: stored as zero length file\n", path );
    }
    return( 0 );

sn_error:
    snet_close( sn );
    exit( 2 );
}

    int 
stor_file( SNET *sn, char *pathdesc, char *path, off_t transize,
    char *trancksum )
{
    int			fd;
    char                *line;
    unsigned char       buf[ 8192 ];
    struct stat         st;
    struct timeval      tv;
    ssize_t             rr, size = 0;
    int			md_len;
    extern EVP_MD       *md;
    EVP_MD_CTX          mdctx;
    unsigned char       md_value[ EVP_MAX_MD_SIZE ];
    unsigned char       cksum_b64[ SZ_BASE64_E( EVP_MAX_MD_SIZE ) ];

    /* Check for checksum in transcript */
    if ( cksum ) {
        if ( strcmp( trancksum, "-" ) == 0 ) {
	    fprintf( stderr, "line %d: No checksum listed\n", linenum );
            return( -1 );
        }
	EVP_DigestInit( &mdctx, md );
    }

    /* Open and stat file */
    if (( fd = open( path, O_RDONLY, 0 )) < 0 ) {
	perror( path );
	return( -1 );
    }
    if ( fstat( fd, &st ) < 0 ) {
	perror( path );
	close( fd );
	return( -1 );
    }

    /* Check size listed in transcript */
    if ( st.st_size != transize ) {
	if ( force ) {
	    fprintf( stderr, "warning: " );
	}
	fprintf( stderr,
	    "line %d: size in transcript does not match size of file\n",
	    linenum );
	if ( ! force ) {
	    close( fd );
	    return( -1 );
	}
    }
    size = st.st_size;

    /* tell server what it is getting */
    if ( snet_writef( sn, "%s", pathdesc ) < 0 ) {
	fprintf( stderr, "store %s failed: %s\n", pathdesc,
	    strerror( errno ));
	goto sn_error;
    }
    if ( verbose ) printf( ">>> %s", pathdesc );
    tv = timeout;
    if (( line = snet_getline_multi( sn, logger, &tv )) == NULL ) {
	if ( snet_eof( sn )) {
	    fprintf( stderr, "store %s failed: Connection closed\n",
		pathdesc );
	} else {
	    fprintf( stderr, "store %s failed: %s\n", pathdesc,
		strerror( errno ));
	}
	goto sn_error;
    }
    if ( *line != '3' ) {
	/* Error from server - transaction aborted */
        fprintf( stderr, "%s\n", line );
	close( fd );
        return( -1 );
    }

    /* tell server how much data to expect */
    if ( snet_writef( sn, "%" PRIofft "d\r\n", st.st_size ) < 0 ) {
	fprintf( stderr, "store %s failed: %s\n", pathdesc,
	    strerror( errno ));
	goto sn_error;
    }
    if ( verbose ) printf( ">>> %" PRIofft "d\n", st.st_size );

    /* write file to server */
    while (( rr = read( fd, buf, sizeof( buf ))) > 0 ) {
	tv = timeout;
	if ( snet_write( sn, buf, rr, &tv ) != rr ) {
	    fprintf( stderr, "store %s failed: %s\n", pathdesc,
		strerror( errno ));
	    goto sn_error;
	}
	size -= rr;
	if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }
	if ( cksum ) {
	    EVP_DigestUpdate( &mdctx, buf, (unsigned int)rr );
	}
    }
    if ( rr < 0 ) {
	perror( path );
	goto sn_error;
    }

    /* Check number of bytes sent to server */
    if ( size != 0 ) {
	fprintf( stderr,
	    "store %s failed: Sent wrong number of bytes to server\n",
	    pathdesc );
	goto sn_error;
    }

    /* End transaction with server */
    if ( snet_writef( sn, ".\r\n" ) < 0 ) {
	fprintf( stderr, "store %s failed: %s\n", pathdesc,
	    strerror( errno ));
	goto sn_error;
    }
    if ( verbose ) fputs( "\n>>> .\n", stdout );
    tv = timeout;
    if (( line = snet_getline_multi( sn, logger, &tv )) == NULL ) {
	if ( snet_eof( sn )) {
	    fprintf( stderr, "store %s failed: Connection closed\n",
		pathdesc );
	} else {
	    fprintf( stderr, "store %s failed: %s\n", pathdesc,
		strerror( errno ));
	}
	goto sn_error;
    }
    if ( *line != '2' ) {
	/* Error from server - transaction aborted */
        fprintf( stderr, "%s\n", line );
	close( fd );
        return( -1 );
    }

    /* Done with server */

    if ( close( fd ) < 0 ) {
	perror( path );
	return( -1 );
    }

    /* cksum data sent */
    if ( cksum ) {
	EVP_DigestFinal( &mdctx, md_value, &md_len );
	base64_e( ( char*)&md_value, md_len, cksum_b64 );
        if ( strcmp( trancksum, cksum_b64 ) != 0 ) {
	    fprintf( stderr,
		"line %d: checksum listed in transcript wrong\n", linenum );
	    if ( ! force ) return( -1 );
        }
    }


    if ( !quiet && !verbose ) printf( "%s: stored\n", path );
    return( 0 );

sn_error:
    snet_close( sn );
    exit( 2 );
}

#ifdef __APPLE__
    int    
stor_applefile( SNET *sn, char *pathdesc, char *path, off_t transize, 
    char *trancksum, struct applefileinfo *afinfo )
{
    int			rc = 0, dfd = 0, rfd = 0;
    off_t		size;
    char		buf[ 8192 ];
    char	        *line;
    struct timeval   	tv;
    int		      	md_len;
    extern EVP_MD      	*md;
    EVP_MD_CTX         	mdctx;
    unsigned char      	md_value[ EVP_MAX_MD_SIZE ];
    unsigned char	cksum_b64[ EVP_MAX_MD_SIZE ];

    /* Check for checksum in transcript */
    if ( cksum ) {
        if ( strcmp( trancksum, "-" ) == 0 ) {
	    fprintf( stderr, "line %d: No checksum listed\n", linenum );
            return( -1 );
        }
        EVP_DigestInit( &mdctx, md );
    }

    /* Check size listed in transcript */
    if ( afinfo->as_size != transize ) {
	if ( force ) {
	    fprintf( stderr, "warning: " );
	}
	fprintf( stderr,
	    "%s: size in transcript does not match size of file\n",
	    decode( path ));
	if ( ! force ) return( -1 );
    }
    size = afinfo->as_size;

    /* open data and rsrc fork */
    if (( dfd = open( path, O_RDONLY )) < 0 ) {
	perror( path );
	return( -1 );
    }
    if ( afinfo->as_ents[ AS_RFE ].ae_length > 0 ) {
	if (( rfd = open( afinfo->rsrc_path, O_RDONLY )) < 0 ) {
	    perror( afinfo->rsrc_path );
	    close( dfd );
	    return( -1 );
	}
    }
    if ( snet_writef( sn, "%s",	pathdesc ) < 0 ) {
	fprintf( stderr, "store %s failed: %s\n", pathdesc,
	    strerror( errno ));
	goto sn_error;
    }
    if ( verbose ) {
	printf( ">>> %s", pathdesc );
    }

    /* tell server what it is getting */
    tv = timeout;
    if (( line = snet_getline_multi( sn, logger, &tv )) == NULL ) {
	if ( snet_eof( sn )) {
	    fprintf( stderr, "store %s failed: Connection closed\n",
		pathdesc );
	} else {
	    fprintf( stderr, "store %s failed: %s\n", pathdesc,
		strerror( errno ));
	}
	goto sn_error;
    }
    if ( *line != '3' ) {
	/* Error from server - transaction aborted */
        fprintf( stderr, "%s\n", line );
	close( dfd );
	if ( afinfo->as_ents[ AS_RFE ].ae_length > 0 ) {
	    close( rfd );
	}
        return( -1 );
    }

    /* tell server how much data to expect */
    tv = timeout;
    if ( snet_writef( sn, "%" PRIofft "d\r\n", afinfo->as_size ) < 0 ) {
	fprintf( stderr, "store %s failed: %s\n", pathdesc,
	    strerror( errno ));
        goto sn_error;
    }
    if ( verbose ) printf( ">>> %" PRIofft "d\n", afinfo->as_size );

    /* write applesingle header to server */
    tv = timeout;
    if ( snet_write( sn, ( char * )&as_header, AS_HEADERLEN, &tv ) !=
		AS_HEADERLEN  ) {
	fprintf( stderr, "store %s failed: %s\n", pathdesc,
	    strerror( errno ));
	goto sn_error;
    }
    size -= AS_HEADERLEN;
    if ( cksum ) {
	EVP_DigestUpdate( &mdctx, (char *)&as_header, AS_HEADERLEN );
    }
    if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }

    /* write header entries to server */
    tv = timeout;
    if ( snet_write( sn, ( char * )&afinfo->as_ents,
		( 3 * sizeof( struct as_entry )), &tv )
		!= ( 3 * sizeof( struct as_entry ))) {
	fprintf( stderr, "store %s failed: %s\n", pathdesc,
	    strerror( errno ));
	goto sn_error;
    }
    size -= ( 3 * sizeof( struct as_entry ));
    if ( cksum ) {
	EVP_DigestUpdate( &mdctx, (char *)&afinfo->as_ents,
	    (unsigned int)( 3 * sizeof( struct as_entry )));
    }
    if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }

    /* write finder info data to server */
    tv = timeout;
    if ( snet_write( sn, afinfo->ai.ai_data, FINFOLEN, &tv ) != FINFOLEN ) {
	fprintf( stderr, "store %s failed: %s\n", pathdesc,
	    strerror( errno ));
	goto sn_error;
    }
    size -= FINFOLEN;
    if ( cksum ) {
	EVP_DigestUpdate( &mdctx, afinfo->ai.ai_data, FINFOLEN );
    }
    if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }

    /* write rsrc fork data to server */
    if ( afinfo->as_ents[ AS_RFE ].ae_length > 0 ) {
	while (( rc = read( rfd, buf, sizeof( buf ))) > 0 ) {
	    tv = timeout;
	    if ( snet_write( sn, buf, rc, &tv ) != rc ) {
		fprintf( stderr, "store %s failed: %s\n", pathdesc,
		    strerror( errno ));
		goto sn_error;
	    }
	    size -= rc;
	    if ( cksum ) {
		EVP_DigestUpdate( &mdctx, buf, (unsigned int)rc );
	    } 
	    if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }
	}
	if ( rc < 0 ) {
	    fprintf( stderr, "store %s failed: %s\n", pathdesc,
		strerror( errno ));
	    goto sn_error;
	}
    }

    /* write data fork to server */
    while (( rc = read( dfd, buf, sizeof( buf ))) > 0 ) {
	tv = timeout;
	if ( snet_write( sn, buf, rc, &tv ) != rc ) {
	    fprintf( stderr, "store %s failed: %s\n", pathdesc,
		strerror( errno ));
	    goto sn_error;
	}
	size -= rc;
	if ( cksum ) {
	    EVP_DigestUpdate( &mdctx, buf, (unsigned int)rc );
	}
    	if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }
    }
    if ( rc < 0 ) {
	fprintf( stderr, "store %s failed: %s\n", pathdesc,
	    strerror( errno ));
	goto sn_error;
    }

    /* Check number of bytes sent to server */
    if ( size != 0 ) {
        fprintf( stderr,
            "store %s failed: Sent wrong number of bytes to server\n",
            pathdesc );
        goto sn_error;
    }

    /* End transaction with server */
    if ( snet_writef( sn, ".\r\n" ) < 0 ) {
	fprintf( stderr, "store %s failed: %s\n", pathdesc,
	    strerror( errno ));
	goto sn_error;
    }
    if ( verbose ) fputs( "\n>>> .\n", stdout );
    tv = timeout;
    if (( line = snet_getline_multi( sn, logger, &tv )) == NULL ) {
	if ( snet_eof( sn )) {
	    fprintf( stderr, "store %s failed: Connection closed\n",
		pathdesc );
	} else {
	    fprintf( stderr, "store %s failed: %s\n", pathdesc,
		strerror( errno ));
	}
	goto sn_error;
    }
    if ( *line != '2' ) {
	/* Error from server - transaction aborted */
        fprintf( stderr, "%s\n", line );
        return( -1 );
    }

    /* Done with server */

    /* Close file descriptors */
    if ( close( dfd ) < 0 ) {
	perror( path );
	if ( afinfo->as_ents[ AS_RFE ].ae_length > 0 ) {
	    close( rfd );
	}
	return( -1 );
    }
    if ( afinfo->as_ents[ AS_RFE ].ae_length > 0 ) {
	if ( close( rfd ) < 0 ) {
	    perror( afinfo->rsrc_path );
	    return( -1 );
	}
    }

    /* cksum data sent */
    if ( cksum ) {
        EVP_DigestFinal( &mdctx, md_value, &md_len );
        base64_e( ( char*)&md_value, md_len, cksum_b64 );
        if ( strcmp( trancksum, cksum_b64 ) != 0 ) {
	    fprintf( stderr,
		"line %d: checksum listed in transcript wrong\n", linenum );
	    if ( ! force ) return( -1 );
        }
    }

    if ( !quiet && !verbose ) printf( "%s: stored\n", decode( path ));
    return( 0 );

sn_error:
    snet_close( sn );
    exit( 2 );
}

    int
n_stor_applefile( SNET *sn, char *pathdesc, char *path )
{
    struct timeval      	tv;
    char                	*line; 
    struct applefileinfo	afinfo;
    off_t			size;

    /* Setup fake apple file info */
    /* Finder Info */
    memset( &afinfo, 0, sizeof( afinfo ));
    sprintf( afinfo.ai.ai_data + FI_CREATOR_OFFSET, "%s", "RDMD" );
    afinfo.as_ents[AS_FIE].ae_id = ASEID_FINFO;
    afinfo.as_ents[AS_FIE].ae_offset = AS_HEADERLEN +
	( 3 * sizeof( struct as_entry ));               /* 62 */
    afinfo.as_ents[AS_FIE].ae_length = FINFOLEN;

    /* Resource Fork */
    afinfo.as_ents[AS_RFE].ae_id = ASEID_RFORK;
    afinfo.as_ents[AS_RFE].ae_offset =                     /* 94 */
	    ( afinfo.as_ents[ AS_FIE ].ae_offset
	    + afinfo.as_ents[ AS_FIE ].ae_length );
    afinfo.as_ents[ AS_RFE ].ae_length = 0;

    /* Data Fork */
    afinfo.as_ents[AS_DFE].ae_id = ASEID_DFORK;
    afinfo.as_ents[ AS_DFE ].ae_offset =
	( afinfo.as_ents[ AS_RFE ].ae_offset
	+ afinfo.as_ents[ AS_RFE ].ae_length );
    afinfo.as_ents[ AS_DFE ].ae_length = 0;

    afinfo.as_size = afinfo.as_ents[ AS_DFE ].ae_offset
	+ afinfo.as_ents[ AS_DFE ].ae_length;

    size = afinfo.as_size;

    /* tell server what it is getting */
    if ( snet_writef( sn, "%s", pathdesc ) < 0 ) {
	fprintf( stderr, "store %s failed: %s\n", pathdesc,
	    strerror( errno ));
	goto sn_error;
    }
    if ( verbose ) printf( ">>> %s", pathdesc );
    tv = timeout;
    if (( line = snet_getline_multi( sn, logger, &tv )) == NULL ) {
	if ( snet_eof( sn )) {
	    fprintf( stderr, "store %s failed: Connection closed\n",
		pathdesc );
	} else {
	    fprintf( stderr, "store %s failed: %s\n", pathdesc,
		strerror( errno ));
	}
	goto sn_error;
    }
    if ( *line != '3' ) {
	/* Error from server - transaction aborted */
        fprintf( stderr, "%s\n", line );
        return( -1 );
    }

    /* tell server how much data to expect and send '.' */
    if ( snet_writef( sn, "%" PRIofft "d\r\n", afinfo.as_size ) < 0 ) {
	fprintf( stderr, "store %s failed: %s\n", pathdesc,
	    strerror( errno ));
	goto sn_error;
    }
    if ( verbose ) printf( ">>> %" PRIofft "d\n", afinfo.as_size );

    /* write applesingle header to server */
    tv = timeout;
    if ( snet_write( sn, (char *)&as_header, AS_HEADERLEN, &tv )
	    != AS_HEADERLEN ) {
	fprintf( stderr, "store %s failed: %s\n", pathdesc,
	    strerror( errno ));
	goto sn_error;
    }
    size -= AS_HEADERLEN;
    if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }

    /* write header entries to server */
    tv = timeout;
    if ( snet_write( sn, ( char * )&afinfo.as_ents,
		( 3 * sizeof( struct as_entry )), &tv )
		!= ( 3 * sizeof( struct as_entry ))) {
	fprintf( stderr, "store %s failed: %s\n", pathdesc,
	    strerror( errno ));
	goto sn_error;
    }
    size -= ( 3 * sizeof( struct as_entry ));
    if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }

    /* write finder info data to server */
    tv = timeout;
    if ( snet_write( sn, afinfo.ai.ai_data, FINFOLEN, &tv ) != FINFOLEN ) {
	fprintf( stderr, "store %s failed: %s\n", pathdesc,
	    strerror( errno ));
	goto sn_error;
    }
    size -= FINFOLEN;
    if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }

    /* Check number of bytes sent to server */
    if ( size != 0 ) {
        fprintf( stderr,
            "store %s failed: Sent wrong number of bytes to server\n",
            pathdesc );
        goto sn_error;
    }

    /* End transaction with server */
    if ( snet_writef( sn, ".\r\n" ) < 0 ) {
	fprintf( stderr, "store %s failed: %s\n", pathdesc,
	    strerror( errno ));
	goto sn_error;
    }
    if ( verbose ) fputs( "\n>>> .\n", stdout );
    tv = timeout;
    if (( line = snet_getline_multi( sn, logger, &tv )) == NULL ) {
	if ( snet_eof( sn )) {
	    fprintf( stderr, "store %s failed: Connection closed\n",
		pathdesc );
	} else {
	    fprintf( stderr, "store %s failed: %s\n", pathdesc,
		strerror( errno ));
	}
	goto sn_error;
    }
    if ( *line != '2' ) {
	/* Error from server - transaction aborted */
        fprintf( stderr, "%s\n", line );
        return( -1 );
    }

    if ( !quiet && !verbose ) {
        printf( "%s: stored as zero length applefile\n", path );
    }
    return( 0 );

sn_error:
    snet_close( sn );
    exit( 2 );
}

#else /* __APPLE__ */
    int
stor_applefile( SNET *sn, char *pathdesc, char *path, off_t transize, 
    char *trancksum, struct applefileinfo *afinfo )
{
    errno = EINVAL;
    return( -1 );
}

    int
n_stor_file( SNET *sn, char *pathdesc, char *path )
{
    errno = EINVAL;
    return( -1 );
}
#endif /* __APPLE__ */
