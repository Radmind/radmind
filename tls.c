/*
 * Copyright (c) 2003 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>

#include <netinet/in.h>			/* For inet_aton */		
#include <arpa/inet.h>

#include <openssl/ssl.h>
#include <openssl/rand.h>
#include <openssl/err.h>

#include <openssl/x509v3.h>		/* For x509 v3 extensions */
#include <openssl/safestack.h>

#include <string.h>

#ifdef HAVE_ZLIB
#include <zlib.h>
#endif /* HAVE_ZLIB */

#include <snet.h>

/* what kind of hostname were we given? */
#define IS_DNS  0
#define IS_IP4  1

#include "tls.h"
    
int _randfile( void );

extern void            (*logger)( char * );
extern int		verbose;
extern SSL_CTX		*ctx;
extern struct timeval	timeout;

char 			*caFile = NULL;
char 			*caDir = NULL;
char 			*crlFile = NULL;
char 			*crlDir = NULL;
char 			*cert = _RADMIND_TLS_CERT;
char 			*privatekey = _RADMIND_TLS_CERT;

    int
_randfile( void )
{
    char        randfile[ MAXPATHLEN ];

    /* generates a default path for the random seed file */
    if ( RAND_file_name( randfile, sizeof( randfile )) == NULL ) {
	fprintf( stderr, "RAND_file_name: %s\n",
		ERR_error_string( ERR_get_error(), NULL ));
	return( -1 );
    }

    /* reads the complete randfile and adds them to the PRNG */
    if ( RAND_load_file( randfile, -1 ) <= 0 ) {
	fprintf( stderr, "RAND_load_file: %s: %s\n", randfile,
		ERR_error_string( ERR_get_error(), NULL ));
	return( -1 );
    }

    /* writes a number of random bytes (currently 1024) to randfile */
    if ( RAND_write_file( randfile ) < 0 ) {
	fprintf( stderr, "RAND_write_file: %s: %s\n", randfile,
		ERR_error_string( ERR_get_error(), NULL ));
	return( -1 );
    }
    return( 0 );
}

    int
tls_server_setup( int use_randfile, int authlevel, char *caFile, char *caDir, char *crlFile, char *crlDir, char *cert, char *privatekey )
{
    extern SSL_CTX	*ctx;
    int                 ssl_mode = 0;
    X509_STORE          *store = NULL;
#ifdef HAVE_X509_VERIFY_PARAM
    X509_VERIFY_PARAM   *param;
#endif /*HAVE_X509_VERIFY_PARAM*/
    int                 vflags = 0;

    SSL_load_error_strings();
    SSL_library_init();    

    if ( use_randfile ) {
	if ( _randfile( ) != 0 ) {
	    return( -1 );
	}
    }

    /* Setup SSL */
    if (( ctx = SSL_CTX_new( SSLv23_server_method())) == NULL ) {
	fprintf( stderr, "SSL_CTX_new: %s\n",
		ERR_error_string( ERR_get_error(), NULL ));
	return( -1 );
    }

    if ( SSL_CTX_use_PrivateKey_file( ctx, privatekey,
	    SSL_FILETYPE_PEM ) != 1 ) {
	fprintf( stderr, "SSL_CTX_use_PrivateKey_file: %s: %s\n",
		privatekey, ERR_error_string( ERR_get_error(), NULL ));
	return( -1 );
    }
    if ( SSL_CTX_use_certificate_chain_file( ctx, cert ) != 1 ) {
	fprintf( stderr, "SSL_CTX_use_certificate_chain_file: %s: %s\n",
		cert, ERR_error_string( ERR_get_error(), NULL ));
	return( -1 );
    }
    /* Verify that private key matches cert */
    if ( SSL_CTX_check_private_key( ctx ) != 1 ) {
	fprintf( stderr, "SSL_CTX_check_private_key: %s\n",
		ERR_error_string( ERR_get_error(), NULL ));
	return( -1 );
    }

    if ( authlevel >= 2 ) {
	/* Set default CA location of not specified */
	if ( caFile == NULL && caDir == NULL ) {
	    caFile = _RADMIND_TLS_CA;
	}

	/* Load CA */
	if ( caFile != NULL ) {
	    if ( SSL_CTX_load_verify_locations( ctx, caFile, NULL ) != 1 ) {
		fprintf( stderr, "SSL_CTX_load_verify_locations: %s: %s\n",
			caFile, ERR_error_string( ERR_get_error(), NULL ));
		return( -1 );
	    }
	}
	if ( caDir != NULL ) {
	    if ( SSL_CTX_load_verify_locations( ctx, NULL, caDir ) != 1 ) {
		fprintf( stderr, "SSL_CTX_load_verify_locations: %s: %s\n",
			caDir, ERR_error_string( ERR_get_error(), NULL ));
		return( -1 );
	    }
	}
    }

    if ( authlevel >= 3 ) {
        /* Load CRL */
	if ( crlFile != NULL ) {
	    if ( SSL_CTX_load_verify_locations( ctx, crlFile, NULL ) != 1 ) {
		fprintf( stderr, "SSL_CTX_load_verify_locations: %s: %s\n",
			crlFile, ERR_error_string( ERR_get_error(), NULL ));
		return( -1 );
	    }
	}
	if ( crlDir != NULL ) {
	    if ( SSL_CTX_load_verify_locations( ctx, NULL, crlDir ) != 1 ) {
		fprintf( stderr, "SSL_CTX_load_verify_locations: %s: %s\n",
			crlDir, ERR_error_string( ERR_get_error(), NULL ));
		return( -1 );
	    }
	}
    }

    /* Set level of security expectations */
    switch ( authlevel ) {
    case 1:
	ssl_mode = SSL_VERIFY_NONE;
        break;

    case 2:
	ssl_mode = SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
        break;

    case 3:
        ssl_mode = SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
        vflags |= X509_V_FLAG_CRL_CHECK;
        break;

    case 4:
        ssl_mode = SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
        vflags |= X509_V_FLAG_CRL_CHECK|X509_V_FLAG_CRL_CHECK_ALL;
        break;
    }

    store = SSL_CTX_get_cert_store(ctx);

#ifdef HAVE_X509_VERIFY_PARAM
    param = X509_VERIFY_PARAM_new();
    X509_VERIFY_PARAM_set_flags(param, vflags);
    X509_STORE_set1_param(store, param);
    X509_VERIFY_PARAM_free(param);
#else /*HAVE_X509_VERIFY_PARAM*/
    X509_STORE_set_flags(store, vflags);
#endif /*HAVE_X509_VERIFY_PARAM*/

    SSL_CTX_set_verify( ctx, ssl_mode, NULL );

    return( 0 );
}   

    int
tls_client_setup( int use_randfile, int authlevel, char *caFile, char *caDir, char *cert, char *privatekey )
{
    extern SSL_CTX	*ctx;
    int                 ssl_mode = 0;

    /* Setup SSL */

    SSL_load_error_strings();
    SSL_library_init();

    if ( use_randfile ) {
	if ( _randfile( ) != 0 ) {
	    return( -1 );
	}
    }

    if (( ctx = SSL_CTX_new( SSLv23_client_method())) == NULL ) {
	fprintf( stderr, "SSL_CTX_new: %s\n",
		ERR_error_string( ERR_get_error(), NULL ));
	return( -1 );
    }

    if ( authlevel == 2 ) {
	if ( SSL_CTX_use_PrivateKey_file( ctx, privatekey,
		SSL_FILETYPE_PEM ) != 1 ) {
	    fprintf( stderr, "SSL_CTX_use_PrivateKey_file: %s: %s\n",
		   privatekey, ERR_error_string( ERR_get_error(), NULL ));
	    return( -1 );
	}
	if ( SSL_CTX_use_certificate_chain_file( ctx, cert ) != 1 ) {
	    fprintf( stderr, "SSL_CTX_use_certificate_chain_file: %s: %s\n",
		    cert, ERR_error_string( ERR_get_error(), NULL ));
	    return( -1 );
	}
	/* Verify that private key matches cert */
	if ( SSL_CTX_check_private_key( ctx ) != 1 ) {
	    fprintf( stderr, "SSL_CTX_check_private_key: %s\n",
		    ERR_error_string( ERR_get_error(), NULL ));
	    return( -1 );
	}
    }

    /* Set default CA location of not specified */
    if ( caFile == NULL && caDir == NULL ) {
	caFile = _RADMIND_TLS_CA;
    }

    /* Load CA */
    if ( caFile != NULL ) {
	if ( SSL_CTX_load_verify_locations( ctx, caFile, NULL ) != 1 ) {
	    fprintf( stderr, "SSL_CTX_load_verify_locations: %s: %s\n",
		    caFile, ERR_error_string( ERR_get_error(), NULL ));
	    return( -1 );
	}
    }
    if ( caDir != NULL ) {
	if ( SSL_CTX_load_verify_locations( ctx, NULL, caDir ) != 1 ) {
	    fprintf( stderr, "SSL_CTX_load_verify_locations: %s: %s\n",
		    caDir, ERR_error_string( ERR_get_error(), NULL ));
	    return( -1 );
	}
    }

    /* Set level of security expecations */
    ssl_mode = SSL_VERIFY_PEER;
    SSL_CTX_set_verify( ctx, ssl_mode, NULL );

    return( 0 );
}

    int
tls_client_start( SNET *sn, char *host, int authlevel )
{
    X509            	*peer;
    char             	buf[ 1024 ];
    struct timeval  	tv;
    char            	*line;
    int             	ntype;
    struct in_addr  	addr;
    int 		alt_ext;

    if ( inet_aton( host, &addr )) {
	ntype = IS_IP4;
    } else {
	/* Assume the host argument is a DNS name */
	ntype = IS_DNS;
    }

    if( snet_writef( sn, "STARTTLS\r\n" ) < 0 ) {
	perror( "snet_writef" );
	return( -1 );
    }
    if ( verbose ) printf( ">>> STARTTLS\n" );

    /* Check to see if command succeeded */
    tv = timeout;      
    if (( line = snet_getline_multi( sn, logger, &tv )) == NULL ) {
	perror( "snet_getline_multi" );
	return( -1 );
    }
    if ( *line != '2' ) {
	fprintf( stderr, "%s\n",  line );
	return( -1 );
    }

    /*
     * Begin TLS
     */
    /* This is where the TLS start */
    /* At this point the server is also staring TLS */

    if ( snet_starttls( sn, ctx, 0 ) != 1 ) {
	fprintf( stderr, "snet_starttls: %s\n",
		ERR_error_string( ERR_get_error(), NULL ) );
	return( -1 );
    }
    if (( peer = SSL_get_peer_certificate( sn->sn_ssl ))
	    == NULL ) {
	fprintf( stderr, "no certificate\n" );
	return( -1 );
    }

    /* This code gratiously borrowed from openldap-2.2.17,
     * it allows the use of aliases in the certificate.
     */
    alt_ext = X509_get_ext_by_NID( peer, NID_subject_alt_name, -1 );

    if ( alt_ext >= 0 ) {
	X509_EXTENSION			*ex;
	STACK_OF( GENERAL_NAME )		*alt;

	ex = X509_get_ext( peer, alt_ext );
	alt = X509V3_EXT_d2i( ex );

	if ( alt ) {
	    int			i, n, len1 = 0, len2 = 0;
	    char	 	*domain = NULL;
	    GENERAL_NAME	*gn;

	    if ( ntype == IS_DNS ) {
		len1 = strlen( host );
		domain = strchr( host, '.' );
		if ( domain ) {
		    len2 = len1 - ( domain-host );
		}
	    }

	    n = sk_GENERAL_NAME_num( alt );
	    for ( i = 0; i < n; i++ ) {
		char	*sn;
		int	 sl;

		gn = sk_GENERAL_NAME_value( alt, i );
		if ( gn->type == GEN_DNS ) {
		    if ( ntype != IS_DNS ) {
			continue;
		    };
		    sn = (char *) ASN1_STRING_get0_data( gn->d.ia5 );
		    sl = ASN1_STRING_length( gn->d.ia5 );

		    /* ignore empty */
		    if ( sl == 0 ) {
			continue;
		    }

		    /* Is this an exact match? */
		    if (( len1 == sl ) && !strncasecmp( host, sn, len1 )) {
			/* Found! */
			if ( verbose ) {
			    printf( ">>> Certificate accepted: "
				"subjectAltName exact match %s\n", sn );
			}
			break;
		    }

		    /* Is this a wildcard match? */
		    if ( domain && ( sn[0] == '*' ) && ( sn[1] == '.' ) &&
			    ( len2 == sl-1 ) && 
			    strncasecmp( domain, &sn[1], len2 )) {
			/* Found! */
			if ( verbose ) {
			    printf( ">>> Certificate accepted: subjectAltName "
			    "wildcard %s host %s\n", sn, host );
			}
			break;
		    }

		} else if ( gn->type == GEN_IPADD ) {
		    if ( ntype == IS_DNS ) {
			continue;
		    }

		    sn = (char *) ASN1_STRING_get0_data( gn->d.ia5 );
		    sl = ASN1_STRING_length( gn->d.ia5 );

		    if ( ntype == IS_IP4 && sl != sizeof( struct in_addr )) {
			continue;
		    }

		    if ( !memcmp( sn, &addr, sl )) {
			/* Found! */
			if ( verbose ) {
			    printf( ">>> Certificate accepted: subjectAltName "
			    "address %s\n", host );
			}
			break;
		    }

		}
	    }

	    GENERAL_NAMES_free( alt );

	    if ( i < n ) {
		/* Found a match */
		X509_free( peer );
		return 0;
	    }
	}
    }

    X509_NAME_get_text_by_NID( X509_get_subject_name( peer ),
	    NID_commonName, buf, sizeof( buf ));
    X509_free( peer );

    if ( strcmp( buf, host )) {
	fprintf( stderr, "Server's name doesn't match supplied hostname\n"
	    "%s != %s\n", buf, host );
	return( -1 );
    }

    return( 0 );
}
