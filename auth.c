/*
 * Copyright (c) 2000 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include <sys/types.h>
#include <sys/time.h>
#include <syslog.h>
#include <strings.h>

#include "snet.h"
#include "auth.h"

/*
 * SASL Authenticate command.  This handles SASL authentication mechanisms
 * "our way".  This code can't be copied wholesale unless the protocol's
 * SASL authentication mechanism exactly matches the encoding used here.
 */

struct sasl {
    char	*s_name;
    int		(*s_func) ___P(( struct sasl *, SNET *, int, char *[] ));
};

static int	f_auth_anon ___P(( struct sasl *, SNET *, int, char *[] ));
static int	f_auth_krb4 ___P(( struct sasl *, SNET *, int, char *[] ));

    static int
f_auth_anon( s, sn, ac, av )
    struct sasl	*s;
    SNET		*sn;
    int		ac;
    char	*av[];
{
    switch ( ac ) {
    case 2 :
	syslog( LOG_INFO, "auth anonymous" );
	snet_writef( sn, "%d AUTH ANONYMOUS succeeds\r\n", 210 );
	return( 0 );

    case 3 :
	syslog( LOG_INFO, "auth anonymous %s", av[ 2 ] );
	snet_writef( sn, "%d AUTH ANONYMOUS as %s succeeds\r\n", 210,
		av[ 2 ] );
	return( 0 );

    default :
	snet_writef( sn, "%d AUTH ANONYMOUS syntax error\r\n", 511 );
	return( 1 );
    }
}

    static int
f_auth_krb4( s, sn, ac, av )
    struct sasl	*s;
    SNET		*sn;
    int		ac;
    char	*av[];
{
    return( 1 );
}

struct sasl	sasl[] = {
    { "ANONYMOUS",	f_auth_anon },
    { "KERBEROS_V4",	f_auth_krb4 },
    { NULL, },
};

    int
f_auth( sn, ac, av )
    SNET		*sn;
    int		ac;
    char	*av[];
{
    struct sasl	*s;

    if ( ac < 2 ) {
	snet_writef( sn, "%d AUTH syntax error\r\n", 510 );
	return( 1 );
    }

    for ( s = sasl; s->s_name != NULL; s++ ) {
	if ( strcasecmp( s->s_name, av[ 1 ] ) == 0 ) {
	    break;
	}
    }
    if ( s->s_name == NULL ) {
	snet_writef( sn, "%d AUTH type %s not supported\r\n", 410, av[ 1 ] );
	return( 1 );
    }

    return( (*s->s_func)( s, sn, ac, av ));
}
