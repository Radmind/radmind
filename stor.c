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

extern struct timeval	timeout;
extern struct as_header as_header;
extern int		verbose;
extern int		quiet;
extern int		dodots;
extern int		cksum;
extern int		linenum;
extern int		force;
extern int		showprogress;
extern off_t		lsize, total;
extern void            	(*logger)( char * );
extern SSL_CTX  	*ctx;

    int
stor_response( SNET *sn, int *respcount, struct timeval *tv )
{
    fd_set		fds;
    char		*line;
    struct timeval	to;

    for ( ; *respcount > 0; (*respcount)-- ) {
	if ( ! snet_hasdata( sn )) {
	    FD_ZERO( &fds );
	    FD_SET( snet_fd( sn ), &fds );

	    if ( select( snet_fd( sn ) + 1, &fds, NULL, NULL, tv ) < 0 ) {
		return( -1 );
	    }
	    if ( ! FD_ISSET( snet_fd( sn ), &fds )) {
		break;
	    }
	}

	to = timeout;
	if (( line = snet_getline_multi( sn, logger, &to )) == NULL ) {
	    if ( snet_eof( sn )) {
		fprintf( stderr, "store failed: Connection closed\n" );
	    } else {
		fprintf( stderr, "store failed: %s\n", strerror( errno ));
	    }
	    return( -1 );
	}

	if ( (*respcount) % 2 ) {
	    if ( *line != '2' ) {
		/* Error from server - transaction aborted */
		fprintf( stderr, "%s\n", line );
		return( -1 );
	    }
	} else {
	    if ( *line != '3' ) {
		/* Error from server - transaction aborted */
		fprintf( stderr, "%s\n", line );
		return( -1 );
	    }
	}
    }
    return( 0 );
}

    int
n_stor_file( SNET *sn, char *pathdesc, char *path )
{
    /* tell server what it is getting */
    if ( snet_writef( sn, "%s\r\n", pathdesc ) < 0 ) {
	fprintf( stderr, "n_store_file %s failed: %s\n", pathdesc,
	    strerror( errno ));
	exit( 2 );
    }
    if ( verbose ) printf( ">>> %s\n", pathdesc );

    /* tell server how much data to expect and send '.' */
    if ( snet_writef( sn, "0\r\n.\r\n" ) < 0 ) {
	fprintf( stderr, "n_store_file %s failed: %s\n", pathdesc,
	    strerror( errno ));
	exit( 2 );
    }
    if ( verbose ) fputs( ">>> 0\n>>> .\n", stdout );

    if ( !quiet && !showprogress ) {
        printf( "%s: stored as zero length file\n", path );
    }
    return( 0 );
}

    int 
stor_file( SNET *sn, char *pathdesc, char *path, off_t transize,
    char *trancksum )
{
    int			fd;
    char 	      	buf[ 8192 ];
    struct stat         st;
    struct timeval      tv;
    ssize_t             rr, size = 0;
    unsigned int	md_len;
    extern EVP_MD       *md;
    EVP_MD_CTX          *mdctx = EVP_MD_CTX_new();
    unsigned char       md_value[ SZ_BASE64_D( SZ_BASE64_E( EVP_MAX_MD_SIZE ) ) ];
    char                cksum_b64[ SZ_BASE64_E( EVP_MAX_MD_SIZE ) ];

    /* Check for checksum in transcript */
    if ( cksum ) {
        if ( strcmp( trancksum, "-" ) == 0 ) {
	    fprintf( stderr, "line %d: No checksum listed\n", linenum );
	    exit( 2 );
        }
	EVP_DigestInit( mdctx, md );
    }

    /* Open and stat file */
    if (( fd = open( path, O_RDONLY, 0 )) < 0 ) {
	perror( path );
	exit( 2 );
    }
    if ( fstat( fd, &st ) < 0 ) {
	perror( path );
	close( fd );
	exit( 2 );
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
	    exit( 2 );
	}
    }
    size = st.st_size;

    /* tell server what it is getting */
    if ( snet_writef( sn, "%s\r\n", pathdesc ) < 0 ) {
	fprintf( stderr, "stor_file %s failed: %s\n", pathdesc,
	    strerror( errno ));
	exit( 2 );
    }
    if ( verbose ) printf( ">>> %s\n", pathdesc );

    /* tell server how much data to expect */
    if ( snet_writef( sn, "%" PRIofft "d\r\n", st.st_size ) < 0 ) {
	fprintf( stderr, "stor_file %s failed: %s\n", pathdesc,
	    strerror( errno ));
	exit( 2 );
    }
    if ( verbose ) printf( ">>> %" PRIofft "d\n", st.st_size );

    /* write file to server */
    while (( rr = read( fd, buf, sizeof( buf ))) > 0 ) {
	tv = timeout;
	if ( snet_write( sn, buf, rr, &tv ) != rr ) {
	    fprintf( stderr, "stor_file %s failed: %s\n", pathdesc,
		strerror( errno ));
	    return( -1 );
	}
	size -= rr;
	if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }
	if ( cksum ) {
	    EVP_DigestUpdate( mdctx, buf, (unsigned int)rr );
	}
	
	if ( showprogress ) {
	    progressupdate( rr, path );
	}
    }
    if ( rr < 0 ) {
	perror( path );
	exit( 2 );
    }

    /* Check number of bytes sent to server */
    if ( size != 0 ) {
	fprintf( stderr,
	    "stor_file %s failed: Sent wrong number of bytes to server\n",
	    pathdesc );
	exit( 2 );
    }

    /* End transaction with server */
    if ( snet_writef( sn, ".\r\n" ) < 0 ) {
	fprintf( stderr, "stor_file %s failed: %s\n", pathdesc,
	    strerror( errno ));
	return( -1 );
    }
    if ( verbose ) fputs( "\n>>> .\n", stdout );

    if ( close( fd ) < 0 ) {
	perror( path );
	exit( 2 );
    }

    /* cksum data sent */
    if ( cksum ) {
	EVP_DigestFinal( mdctx, md_value, &md_len );
	base64_e( md_value, md_len, cksum_b64 );
	EVP_MD_CTX_free(mdctx);
        if ( strcmp( trancksum, cksum_b64 ) != 0 ) {
	    fprintf( stderr,
		"line %d: checksum listed in transcript wrong\n", linenum );
	    if ( ! force ) exit( 2 );
        }
    }

    if ( !quiet && !showprogress ) printf( "%s: stored\n", path );
    return( 0 );
}

#ifdef __APPLE__
    int    
stor_applefile( SNET *sn, char *pathdesc, char *path, off_t transize, 
    char *trancksum, struct applefileinfo *afinfo )
{
    int			rc = 0, dfd = 0, rfd = 0;
    off_t		size;
    char		buf[ 8192 ], rsrc_path[ MAXPATHLEN ];
    struct timeval   	tv;
    unsigned int      	md_len;
    unsigned int	rsrc_len;
    extern EVP_MD      	*md;
    EVP_MD_CTX         	*mdctx = EVP_MD_CTX_new();
    unsigned char 	md_value[ SZ_BASE64_D( SZ_BASE64_E( EVP_MAX_MD_SIZE ) ) ];
    char		cksum_b64[ SZ_BASE64_E( EVP_MAX_MD_SIZE ) ];

    /* Check for checksum in transcript */
    if ( cksum ) {
        if ( strcmp( trancksum, "-" ) == 0 ) {
	    fprintf( stderr, "line %d: No checksum listed\n", linenum );
	    exit( 2 );
        }
        EVP_DigestInit( mdctx, md );
    }

    /* Check size listed in transcript */
    if ( afinfo->as_size != transize ) {
	if ( force ) {
	    fprintf( stderr, "warning: " );
	}
	fprintf( stderr,
	    "%s: size in transcript does not match size of file\n", path );
	if ( ! force ) {
	    exit( 2 );
	}
    }
    size = afinfo->as_size;

    /* endian handling, swap header entries if necessary */
    rsrc_len = afinfo->as_ents[ AS_RFE ].ae_length;
    as_entry_netswap( &afinfo->as_ents[ AS_FIE ] );
    as_entry_netswap( &afinfo->as_ents[ AS_RFE ] );
    as_entry_netswap( &afinfo->as_ents[ AS_DFE ] );

    /* open data and rsrc fork */
    if (( dfd = open( path, O_RDONLY )) < 0 ) {
	perror( path );
	exit( 2 );
    }
    if ( rsrc_len > 0 ) {
        if ( snprintf( rsrc_path, MAXPATHLEN, "%s%s",
		path, _PATH_RSRCFORKSPEC ) >= MAXPATHLEN ) {
            errno = ENAMETOOLONG;
            return( -1 );
        }
	if (( rfd = open( rsrc_path, O_RDONLY )) < 0 ) {
	    perror( rsrc_path );
	    close( dfd );
	    exit( 2 );
	}
    }
    if ( snet_writef( sn, "%s\r\n", pathdesc ) < 0 ) {
	fprintf( stderr, "stor_applefile %s failed: %s\n", pathdesc,
	    strerror( errno ));
	exit( 2 );
    }
    if ( verbose ) {
	printf( ">>> %s\n", pathdesc );
    }

    /* tell server how much data to expect */
    tv = timeout;
    if ( snet_writef( sn, "%" PRIofft "d\r\n", afinfo->as_size ) < 0 ) {
	fprintf( stderr, "stor_applefile %s failed: %s\n", pathdesc,
	    strerror( errno ));
	exit( 2 );
    }
    if ( verbose ) printf( ">>> %" PRIofft "d\n", afinfo->as_size );

    /* write applesingle header to server */
    tv = timeout;
    if ( snet_write( sn, ( char * )&as_header, AS_HEADERLEN, &tv ) !=
		AS_HEADERLEN  ) {
	fprintf( stderr, "stor_applefile %s failed: %s\n", pathdesc,
	    strerror( errno ));
	return( -1 );
    }
    size -= AS_HEADERLEN;
    if ( cksum ) {
	EVP_DigestUpdate( mdctx, (char *)&as_header, AS_HEADERLEN );
    }
    if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }
    if ( showprogress ) {
	progressupdate( AS_HEADERLEN, path );
    }

    /* write header entries to server */
    tv = timeout;
    if ( snet_write( sn, ( char * )&afinfo->as_ents,
		( 3 * sizeof( struct as_entry )), &tv )
		!= ( 3 * sizeof( struct as_entry ))) {
	fprintf( stderr, "stor_applefile %s failed: %s\n", pathdesc,
	    strerror( errno ));
	return( -1 );
    }
    size -= ( 3 * sizeof( struct as_entry ));
    if ( cksum ) {
	EVP_DigestUpdate( mdctx, (char *)&afinfo->as_ents,
	    (unsigned int)( 3 * sizeof( struct as_entry )));
    }
    if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }
    if ( showprogress ) {
	progressupdate(( 3 * sizeof( struct as_entry )), path );
    }

    /* write finder info data to server */
    tv = timeout;
    if ( snet_write( sn, (char *)afinfo->ai.ai_data, FINFOLEN,
	    &tv ) != FINFOLEN ) {
	fprintf( stderr, "stor_applefile %s failed: %s\n", pathdesc,
	    strerror( errno ));
	return( -1 );
    }
    size -= FINFOLEN;
    if ( cksum ) {
	EVP_DigestUpdate( mdctx, afinfo->ai.ai_data, FINFOLEN );
    }
    if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }
    if ( showprogress ) {
	progressupdate( FINFOLEN, path );
    }

    /* write rsrc fork data to server */
    if ( rsrc_len > 0 ) {
	while (( rc = read( rfd, buf, sizeof( buf ))) > 0 ) {
	    tv = timeout;
	    if ( snet_write( sn, buf, rc, &tv ) != rc ) {
		fprintf( stderr, "stor_applefile %s failed: %s\n", pathdesc,
		    strerror( errno ));
		return( -1 );
	    }
	    size -= rc;
	    if ( cksum ) {
		EVP_DigestUpdate( mdctx, buf, (unsigned int)rc );
	    } 
	    if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }
	    if ( showprogress ) {
		progressupdate( rc, path );
	    }
	}
	if ( rc < 0 ) {
	    fprintf( stderr, "stor_applefile %s failed: %s\n", pathdesc,
		strerror( errno ));
	    exit( 2 );
	}
    }

    /* write data fork to server */
    while (( rc = read( dfd, buf, sizeof( buf ))) > 0 ) {
	tv = timeout;
	if ( snet_write( sn, buf, rc, &tv ) != rc ) {
	    fprintf( stderr, "stor_applefile %s failed: %s\n", pathdesc,
		strerror( errno ));
	    return( -1 );
	}
	size -= rc;
	if ( cksum ) {
	    EVP_DigestUpdate( mdctx, buf, (unsigned int)rc );
	}
    	if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }
	if ( showprogress ) {
	    progressupdate( rc, path );
        }
    }
    if ( rc < 0 ) {
	fprintf( stderr, "stor_applefile %s failed: %s\n", pathdesc,
	    strerror( errno ));
	exit( 2 );
    }

    /* Check number of bytes sent to server */
    if ( size != 0 ) {
        fprintf( stderr,
            "stor_applefile %s failed: Sent wrong number of bytes to server\n",
            pathdesc );
	exit( 2 );
    }

    /* End transaction with server */
    if ( snet_writef( sn, ".\r\n" ) < 0 ) {
	fprintf( stderr, "stor_applefile %s failed: %s\n", pathdesc,
	    strerror( errno ));
	return( -1 );
    }
    if ( verbose ) fputs( "\n>>> .\n", stdout );

    /* Close file descriptors */
    if ( close( dfd ) < 0 ) {
	perror( path );
	if ( afinfo->as_ents[ AS_RFE ].ae_length > 0 ) {
	    close( rfd );
	}
	exit( 2 );
    }
    if ( afinfo->as_ents[ AS_RFE ].ae_length > 0 ) {
	if ( close( rfd ) < 0 ) {
	    perror( rsrc_path );
	    exit( 2 );
	}
    }

    /* cksum data sent */
    if ( cksum ) {
        EVP_DigestFinal( mdctx, md_value, &md_len );
        base64_e( ( char*)&md_value, md_len, cksum_b64 );
	EVP_MD_CTX_free(mdctx);
        if ( strcmp( trancksum, cksum_b64 ) != 0 ) {
	    fprintf( stderr,
		"line %d: checksum listed in transcript wrong\n", linenum );
	    if ( ! force ) exit( 2 );
        }
    }

    if ( !quiet && !showprogress ) printf( "%s: stored\n", path );
    return( 0 );
}

    int
n_stor_applefile( SNET *sn, char *pathdesc, char *path )
{
    struct timeval      	tv;
    struct applefileinfo	afinfo;
    off_t			size;

    /* Setup fake apple file info */
    /* Finder Info */
    memset( &afinfo, 0, sizeof( afinfo ));
    sprintf( (char *)(afinfo.ai.ai_data + FI_CREATOR_OFFSET), "%s", "RDMD" );
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
    if ( snet_writef( sn, "%s\r\n", pathdesc ) < 0 ) {
	fprintf( stderr, "n_stor_applefile %s failed: %s\n", pathdesc,
	    strerror( errno ));
	exit( 2 );
    }
    if ( verbose ) printf( ">>> %s\n", pathdesc );

    /* tell server how much data to expect and send '.' */
    if ( snet_writef( sn, "%" PRIofft "d\r\n", afinfo.as_size ) < 0 ) {
	fprintf( stderr, "n_stor_applefile %s failed: %s\n", pathdesc,
	    strerror( errno ));
	exit( 2 );
    }
    if ( verbose ) printf( ">>> %" PRIofft "d\n", afinfo.as_size );

    /* write applesingle header to server */
    tv = timeout;
    if ( snet_write( sn, (char *)&as_header, AS_HEADERLEN, &tv )
	    != AS_HEADERLEN ) {
	fprintf( stderr, "n_stor_applefile %s failed: %s\n", pathdesc,
	    strerror( errno ));
	return( -1 );
    }
    size -= AS_HEADERLEN;
    if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }

    /* endian handling, convert to valid AppleSingle format, if necessary */
    as_entry_netswap( &afinfo.as_ents[ AS_FIE ] );
    as_entry_netswap( &afinfo.as_ents[ AS_RFE ] );
    as_entry_netswap( &afinfo.as_ents[ AS_DFE ] );

    /* write header entries to server */
    tv = timeout;
    if ( snet_write( sn, ( char * )&afinfo.as_ents,
		( 3 * sizeof( struct as_entry )), &tv )
		!= ( 3 * sizeof( struct as_entry ))) {
	fprintf( stderr, "n_stor_applefile %s failed: %s\n", pathdesc,
	    strerror( errno ));
	return( -1 );
    }
    size -= ( 3 * sizeof( struct as_entry ));
    if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }

    /* write finder info data to server */
    tv = timeout;
    if ( snet_write( sn, (char *)afinfo.ai.ai_data, FINFOLEN,
	    &tv ) != FINFOLEN ) {
	fprintf( stderr, "n_stor_applefile %s failed: %s\n", pathdesc,
	    strerror( errno ));
	return( -1 );
    }
    size -= FINFOLEN;
    if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }

    /* Check number of bytes sent to server */
    if ( size != 0 ) {
        fprintf( stderr, "n_stor_applefile %s failed:"
	    " Sent wrong number of bytes to server\n",
            pathdesc );
	exit( 2 );
    }

    /* End transaction with server */
    if ( snet_writef( sn, ".\r\n" ) < 0 ) {
	fprintf( stderr, "n_stor_applefile %s failed: %s\n", pathdesc,
	    strerror( errno ));
	return( -1 );
    }
    if ( verbose ) fputs( "\n>>> .\n", stdout );

    if ( !quiet && !showprogress ) {
	printf( "%s: stored as zero length applefile\n", path );
    }
    return( 0 );
}

#else /* __APPLE__ */
    int
stor_applefile( SNET *sn, char *pathdesc, char *path, off_t transize, 
    char *trancksum, struct applefileinfo *afinfo )
{
    fprintf( stderr, "stor_applefile %s invalid\n", pathdesc );
    exit( 2 );
}

    int
n_stor_applefile( SNET *sn, char *pathdesc, char *path )
{
    fprintf( stderr, "n_stor_applefile %s invalid\n", pathdesc );
    exit( 2 );
}
#endif /* __APPLE__ */
