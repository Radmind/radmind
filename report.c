/*
 * Copyright (c) 2006, 2007 Regents of The University of Michigan.
 * All Rights Reserved. See COPYRIGHT.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <ctype.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <openssl/ssl.h>
#include <openssl/rand.h>
#include <openssl/err.h>

#ifdef HAVE_ZLIB
#include <zlib.h>
#endif /* HAVE_ZLIB */

#include <snet.h>

#include "code.h"
#include "report.h"

extern int		verbose;
extern void		(*logger)( char * );
extern struct timeval	timeout;

/*
 * report_event: report an arbitrary event to the server
 *
 * return codes:
 *	0:	event reported
 *	1:	report failed
 */
    int
report_event( SNET *sn, char *event, char *repodata )
{
    struct timeval	tv;
    char		*line;
    char		*e_repodata;
    int			i, len;

    /* sanity check the event */
    if ( event == NULL ) {
	fprintf( stderr, "report_event: event must be non-NULL\n" );
	return( 1 );
    }
    if (( len = strlen( event )) == 0 ) {
	fprintf( stderr, "report_event: invalid zero-length event\n" );
	return( 1 );
    } else {
	for ( i = 0; i < len; i++ ) {
	    if ( isspace(( int )event[ i ] )) {
		fprintf( stderr, "report_event: event must not "
			"contain whitespace\n" );
		return( 1 );
	    }
	}
    }

    if (( e_repodata = encode( repodata )) == NULL ) {
	fprintf( stderr, "report_event: encode: buffer too small\n" );
	return( 1 );
    }

    if ( snet_writef( sn, "REPO %s %s\r\n", event, e_repodata ) < 0 ) {
	perror( "snet_writef" );
	return( 1 );
    }
    if ( verbose ) printf( ">>> REPO %s %s\n", event, e_repodata );

    tv = timeout;
    if (( line = snet_getline_multi( sn, logger, &tv )) == NULL ) {
	perror( "snet_getline_multi" );
	return( 1 );
    }
    if ( *line != '2' ) {
	fprintf( stderr, "%s\n", line );
	return( 1 );
    }

    return( 0 );
}

/*
 * report_error_and_exit: report an error and exit with the give value
 */
    void
report_error_and_exit( SNET *sn, char *event, char *repodata, int rc )
{
    ( void )report_event( sn, event, repodata );

    exit( rc );
}
