/*
 * Copyright (c) 2002 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#ifdef __APPLE__
#include <sys/paths.h>
#include <sys/attr.h>
#endif __APPLE__
#include <sys/uio.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include <openssl/evp.h>
#include <snet.h>

#include "applefile.h"
#include "connect.h"
#include "cksum.h"
#include "base64.h"
#include "code.h"

extern struct timeval	timeout;
extern struct as_header as_header;
extern struct attrlist  alist;
extern int		verbose;
extern int		quiet;
extern int		dodots;
extern int		cksum;
extern int		linenum;
extern void            (*logger)( char * );

    int
n_stor_file( SNET *sn, char *pathdesc, char *path )
{
    struct timeval      tv;
    char                *line;

    if ( snet_writef( sn, "%s", pathdesc ) == NULL ) {
            perror( "snet_writef" );
            return( -1 );
    }

    if ( verbose ) {
        printf( ">>> %s", pathdesc );
    }

    tv.tv_sec = 120;
    tv.tv_usec = 0;
    if (( line = snet_getline_multi( sn, logger, &tv )) == NULL ) {
        perror( "snet_getline_multi" );
        return( -1 );
    }
    if ( *line != '3' ) {
        fprintf( stderr, "%s\n", line );
        return( -1 );
    }

    if ( snet_writef( sn, "0\r\n.\r\n" ) == NULL ) {
        perror( "snet_writef" );
        return( -1 );
    }
    if ( verbose ) fputs( ">>> 0\n\n>>> .\n", stdout );

    tv.tv_sec = 120;
    tv.tv_usec = 0;
    if (( line = snet_getline_multi( sn, logger, &tv )) == NULL ) {
        perror( "snet_getline_multi" );
        return( -1 );
    }
    if ( *line != '2' ) {
        fprintf( stderr, "%s\n", line );
        return( -1 );
    }

    if ( !quiet && !verbose ) {
        printf( "%s: stored as zero length file\n", path );
    }
    return( 0 );
}

    int 
stor_file( SNET *sn, char *pathdesc, char *path, size_t transize,
    char *trancksum )
{
    int			fd;
    char                *line;
    unsigned char       buf[ 8192 ];
    struct stat         st;
    struct timeval      tv;
    ssize_t             rr;
    int			md_len;
    extern EVP_MD       *md;
    EVP_MD_CTX          mdctx;
    unsigned char       md_value[ EVP_MAX_MD_SIZE ];
    unsigned char       cksum_b64[ SZ_BASE64_E( EVP_MAX_MD_SIZE ) ];

    if ( cksum ) {
        if ( strcmp( trancksum, "-" ) == 0 ) {
            return( -3 );
        }
	EVP_DigestInit( &mdctx, md );
    }

    if (( fd = open( path, O_RDONLY, 0 )) < 0 ) {
	perror( path );
	return( -1 );
    }
    if ( fstat( fd, &st) < 0 ) {
	perror( path );
	return( -1 );
    }

    if ( snet_writef( sn, "%s", pathdesc ) == NULL ) {
	perror( "snet_writef" );
	return( -1 );
    }
    if ( verbose ) printf( ">>> %s", pathdesc );

    tv = timeout;
    if (( line = snet_getline_multi( sn, logger, &tv )) == NULL ) {
        perror( "snet_getline_multi" );
        return( -1 );
    }
    if ( *line != '3' ) {
        fprintf( stderr, "%s\n", line );
        return( -1 );
    }

    /* Check size listed in transcript */
    if ( transize != 0 ) {
	if ( st.st_size != transize ) {
	    fprintf( stderr,
		"%s: size in transcript does not match size of file\n",
		path );
	    return( -1 );
	}
    }

     /* tell server how much data to expect */
    if ( snet_writef( sn, "%d\r\n", (int)st.st_size ) == NULL ) {
	perror( "snet_writef" );
	return( -1 );
    }
    if ( verbose ) printf( ">>> %d\n", (int)st.st_size );

    while (( rr = read( fd, buf, sizeof( buf ))) > 0 ) {
	tv = timeout;
	if ( snet_write( sn, buf, (int)rr, &tv ) != rr ) {
	    perror( "snet_write" );
	    return( -1 );
	}
	if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }
	if ( cksum ) {
	    EVP_DigestUpdate( &mdctx, buf, (unsigned int)rr );
	}
    }
    if ( rr < 0 ) {
	perror( path );
	return( -1 );
    }

    if ( snet_writef( sn, ".\r\n" ) == NULL ) {
        perror( "snet_writef" );
        return( -1 );
    }
    if ( verbose ) fputs( "\n>>> .\n", stdout );

    tv = timeout;
    if (( line = snet_getline_multi( sn, logger, &tv )) == NULL ) {
        perror( "snet_getline_multi" );
        return( -1 );
    }
    if ( *line != '2' ) {
        fprintf( stderr, "%s\n", line );
        return( -1 );
    }

    /* cksum data sent */
    if ( cksum ) {
	EVP_DigestFinal( &mdctx, md_value, &md_len );
	base64_e( ( char*)&md_value, md_len, cksum_b64 );
        if ( strcmp( trancksum, cksum_b64 ) != 0 ) {
            return( -2 );
        }
    }

    if ( close( fd ) < 0 ) {
	perror( path );
	exit( 1 );
    }

    if ( !quiet && !verbose ) printf( "%s: stored\n", path );
    return( 0 );
}

#ifdef __APPLE__
    int    
stor_applefile( SNET *sn, char *pathdesc, char *path, size_t transize, 
    char *trancksum, struct applefileinfo *afinfo )
{
    int			rc, dfd, rfd;
    char		buf[ 8192 ];
    char	        *line;
    struct timeval   	tv;
    int		      	md_len;
    extern EVP_MD      	*md;
    EVP_MD_CTX         	mdctx;
    unsigned char      	md_value[ EVP_MAX_MD_SIZE ];
    unsigned char	cksum_b64[ EVP_MAX_MD_SIZE ];

    if ( cksum ) {
        if ( strcmp( trancksum, "-" ) == 0 ) {
            return( -3 );
        }
        EVP_DigestInit( &mdctx, md );
    }

    if (( dfd = open( path, O_RDONLY )) < 0 ) {
	perror( path );
	goto error1;
    }
    if ( afinfo->as_ents[ AS_RFE ].ae_length > 0 ) {
	if (( rfd = open( afinfo->rsrc_path, O_RDONLY )) < 0 ) {
	    perror( afinfo->rsrc_path );
	    goto error1;
	}
    }

    /* STOR "FILE" <transcript-name> <path> "\r\n" */
    if ( snet_writef( sn, "%s",	pathdesc ) == NULL ) {
	perror( "snet_writef" );
	return( -1 );
    }
    if ( verbose ) {
	printf( ">>> %s", pathdesc );
    }

    tv = timeout;
    if (( line = snet_getline_multi( sn, logger, &tv )) == NULL ) {
        perror( "snet_getline_multi" );
        return( -1 );
    }
    if ( *line != '3' ) {
        fprintf( stderr, "%s\n", line );
        return( -1 );
    }

    /* Check size listed in transcript */
    if ( transize != 0 ) {
	if ( afinfo->as_size != transize ) {
	    fprintf( stderr,
		"%s: size in transcript does not match size of file\n",
		decode( path ));
	    return( -1 );
	}
    }

    /* tell server how much data to expect */
    tv = timeout;
    if ( snet_writef( sn, "%d\r\n", (int)afinfo->as_size ) == NULL ) {
        perror( "snet_writef" );
        goto error2;
    }
    if ( verbose ) printf( ">>> %d\n", (int)afinfo->as_size );

    /* snet write applesingle header to server */
    tv = timeout;
    if ( snet_write( sn, ( char * )&as_header, AS_HEADERLEN, &tv ) !=
		AS_HEADERLEN  ) {
	perror( "snet_write" );
	goto error2;
    }
    if ( cksum ) {
	EVP_DigestUpdate( &mdctx, (char *)&as_header, AS_HEADERLEN );
    }
    if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }

    /* snet write header entries */
    tv = timeout;
    if ( snet_write( sn, ( char * )&afinfo->as_ents,
		( 3 * sizeof( struct as_entry )), &tv )
		!= ( 3 * sizeof( struct as_entry ))) {
	perror( "snet_write" );
	goto error2;
    }
    if ( cksum ) {
	EVP_DigestUpdate( &mdctx, (char *)&afinfo->as_ents,
	    (unsigned int)( 3 * sizeof( struct as_entry )));
    }
    if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }

    /* snet_write finder info data to server */
    tv = timeout;
    if ( snet_write( sn, afinfo->fi.fi_data, FINFOLEN, &tv ) != FINFOLEN ) {
	perror( "snet_write" );
	goto error2;
    }
    if ( cksum ) {
	EVP_DigestUpdate( &mdctx, afinfo->fi.fi_data, FINFOLEN );
    }
    if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }

    /* snet_write rsrc fork data to server */
    if ( afinfo->as_ents[ AS_RFE ].ae_length > 0 ) {
	while (( rc = read( rfd, buf, sizeof( buf ))) > 0 ) {
	    tv = timeout;
	    if ( snet_write( sn, buf, rc, &tv ) != rc ) {
		perror( "snet_write" );
		goto error2;
	    }
	    if ( cksum ) {
		EVP_DigestUpdate( &mdctx, buf, (unsigned int)rc );
	    } 
	    if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }
	}
	if ( close( rfd ) < 0 ) {
	    perror( afinfo->rsrc_path );
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
	if ( cksum ) {
	    EVP_DigestUpdate( &mdctx, buf, (unsigned int)rc );
	}
    	if ( dodots ) { putc( '.', stdout ); fflush( stdout ); }
    }
    if ( snet_writef( sn, ".\r\n" ) == NULL ) {
        perror( "snet_writef" );
        return( -1 );
    }
    if ( verbose ) fputs( "\n>>> .\n", stdout );

    if ( close( dfd ) < 0 ) {
	perror( path );
	exit( 1 );
    }

    tv = timeout;
    if (( line = snet_getline_multi( sn, logger, &tv )) == NULL ) {
        perror( "snet_getline_multi" );
        return( -1 );
    }
    if ( *line != '2' ) {
        fprintf( stderr, "%s\n", line );
        return( -1 );
    }

    /* cksum data sent */
    if ( cksum ) {
        EVP_DigestFinal( &mdctx, md_value, &md_len );
        base64_e( ( char*)&md_value, md_len, cksum_b64 );
        if ( strcmp( trancksum, cksum_b64 ) != 0 ) {
            return( -2 );
        }
    }

    if ( !quiet && !verbose ) printf( "%s: stored\n", decode( path ));
    return( 0 );

error1:
    return( -1 );
error2:
    if ( close( rfd ) < 0 ) {
	perror( afinfo->rsrc_path );
	exit( 1 );
    }
    return( -1 );
}
#else !__APPLE__
    int
stor_applefile( SNET *sn, char *pathdesc, char *path, size_t transize, 
    char *trancksum, struct applefileinfo *afinfo )
{
    return( -1 );
}
#endif __APPLE__