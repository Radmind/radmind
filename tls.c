/*
 * Copyright (c) 2003 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <openssl/ssl.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <string.h>

#include <snet.h>

#include "tls.h"
    
int _randfile( void );

extern void            (*logger)( char * );
extern int		verbose;
extern SSL_CTX		*ctx;
extern struct timeval	timeout;

char 			*ca = _RADMIND_TLS_CA;
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
tls_server_setup( int use_randfile, int authlevel, char *ca, char *cert, char *privatekey )
{
    extern SSL_CTX	*ctx;
    int                 ssl_mode = 0;

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

    if ( authlevel == 2 ) {
    /* Load CA */
	if ( SSL_CTX_load_verify_locations( ctx, ca, NULL ) != 1 ) {
	    fprintf( stderr, "SSL_CTX_load_verify_locations: %s: %s\n",
		    ca, ERR_error_string( ERR_get_error(), NULL ));
	    return( -1 );
	}
    }
    /* Set level of security expecations */
    if ( authlevel == 1 ) {
	ssl_mode = SSL_VERIFY_NONE; 
    } else {
	/* authlevel == 2 */
	ssl_mode = SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
    }
    SSL_CTX_set_verify( ctx, ssl_mode, NULL );

    return( 0 );
}   

    int
tls_client_setup( int use_randfile, int authlevel, char *ca, char *cert, char *privatekey )
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

    /* Load CA */
    if ( SSL_CTX_load_verify_locations( ctx, ca, NULL ) != 1 ) {
	fprintf( stderr, "SSL_CTX_load_verify_locations: %s: %s\n",
		ca, ERR_error_string( ERR_get_error(), NULL ));
	return( -1 );
    }

    /* Set level of security expecations */
    ssl_mode = SSL_VERIFY_PEER;
    SSL_CTX_set_verify( ctx, ssl_mode, NULL );

    return( 0 );
}

    int
tls_client_start( SNET *sn, char *host, int authlevel )
{
    X509            *peer;
    char             buf[ 1024 ];
    struct timeval  tv;
    char            *line;

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
