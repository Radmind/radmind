/*
 * Copyright (c) 1998 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include <sys/time.h>
#include <sys/param.h>
#include <sys/types.h>
#include <netdb.h>
#include <string.h>
#include <syslog.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <netinet/in.h>

#include <net.h>

#include "argcargv.h"
#include "command.h"
#include "auth.h"

int		f_quit ___P(( NET *, int, char *[] ));
int		f_noop ___P(( NET *, int, char *[] ));
int		f_help ___P(( NET *, int, char *[] ));
int		f_stat ___P(( NET *, int, char *[] ));
int		f_retr ___P(( NET *, int, char *[] ));
int		f_stor ___P(( NET *, int, char *[] ));

    int
f_quit( net, ac, av )
    NET		*net;
    int		ac;
    char	*av[];
{
    net_writef( net, "%d QUIT OK, closing connection\r\n", 201 );
    exit( 0 );
}

    int
f_noop( net, ac, av )
    NET		*net;
    int		ac;
    char	*av[];
{
    net_writef( net, "%d NOOP OK\r\n", 202 );
    return( 0 );
}

    int
f_help( net, ac, av )
    NET		*net;
    int		ac;
    char	*av[];
{
    net_writef( net, "%d What is this, SMTP?\r\n", 203 );
    return( 0 );
}

struct command	commands[] = {
    { "QUIT",		f_quit },
    { "NOOP",		f_noop },
    { "HELP",		f_help },
    { "AUTHenticate",	f_auth },
    { "STATus",		f_noop },
    { "RETRieve",	f_noop },
    { "STORe",		f_noop },
};

int		ncommands = sizeof( commands ) / sizeof( commands[ 0 ] );
char		hostname[ MAXHOSTNAMELEN ];

    int
cmdloop( fd )
    int		fd;
{
    NET			*net;
    int			ac, i;
    unsigned int	n;
    char		**av, *line;
    struct timeval	tv;
    extern char		*version;

    if (( net = net_attach( fd, 1024 * 1024 )) == NULL ) {
	syslog( LOG_ERR, "net_attach: %m" );
	exit( 1 );
    }

    if ( gethostname( hostname, MAXHOSTNAMELEN ) < 0 ) {
	syslog( LOG_ERR, "gethostname: %m" );
	exit( 1 );
    }

    net_writef( net, "%d RAP 1 %s %s radmind access protocol\r\n", 200,
	    hostname, version );

    tv.tv_sec = 60 * 10;	/* 10 minutes */
    tv.tv_usec = 60 * 10;
    while (( line = net_getline( net, &tv )) != NULL ) {
	tv.tv_sec = 60 * 10;
	tv.tv_usec = 60 * 10;
	if (( ac = argcargv( line, &av )) < 0 ) {
	    syslog( LOG_ERR, "argcargv: %m" );
	    return( 1 );
	}

	if ( ac ) {
	    for ( i = 0; i < ncommands; i++ ) {
#define MAX(a,b)	((a)>(b)?(a):(b))
		n = MAX( strlen( av[ 0 ] ), 4 );
		if ( strncasecmp( av[ 0 ], commands[ i ].c_name, n ) == 0 ) {
		    if ( (*(commands[ i ].c_func))( net, ac, av ) < 0 ) {
			return( 1 );
		    }
		    break;
		}
	    }
	    if ( i >= ncommands ) {
		net_writef( net, "%d Command %s unregcognized\r\n", 500,
			av[ 0 ] );
	    }
	} else {
	    net_writef( net, "%d Illegal null command\r\n", 501 );
	}
    }

    return( 0 );
}
