#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <strings.h>

#include "snet.h"
#include "download.h"
#include "connect.h"
#include "argcargv.h"

#define MIN(X, Y) ((X) < (Y) ? (X) : (Y))

void output( char* string);

void			(*logger)( char * ) = NULL;
extern struct timeval	timeout;
int			verbose = 0;
int			chksum = 0;
char			fullpath[ MAXPATHLEN ];
char			*diff = "/usr/local/gnu/bin/diff";
char			*location = "/tmp/lfdiff";

    void
output( char *string )
{
    printf( "<<< %s\n", string );
    return;
}

/*
 * exit codes:
 *      0       No changes found, everything okay
 *      -1       System error
 */

    int
main( int argc, char **argv, char **envp )
{
    int			c, i, tac, port = htons( 6662 ), err = 0;
    int			special = 0, diffargc = 0;
    int			fd;
    extern int          optind, optopt;
    extern char		*version;
    char		*host = NULL;
    char		*temppath = NULL;
    char		*transcript = NULL;
    char		*file = NULL;
    char		**diffargv;
    char		**argcargv;
    char 		pathdesc[ 2 * MAXPATHLEN ];
    char		opt[ 3 ];
    struct servent	*se;
    SNET		*sn;

    /* create argv to pass to diff */
    if ( ( diffargv = (char **)malloc( 1  * sizeof( char * ) ) ) == NULL ) {
	perror( "malloc" );
	exit( -1 );
    }
    diffargc = 0;
    diffargv[ diffargc++ ] = diff;

    while ( ( c = getopt ( argc, argv, "h:p:ST:VvbitwcefnC:D:sX:" ) )
	    != EOF ) {
	switch( c ) {
	case 'h':
	    host = optarg;
	    break;
	case 'p':
	    if ( ( port = htons ( atoi( optarg ) ) ) == 0 ) {
		if ( ( se = getservbyname( optarg, "tcp" ) ) == NULL ) {
		    fprintf( stderr, "%s: service unkown\n", optarg );
		    exit( -1 );
		}
		port = se->s_port;
	    }
	    break;
	case 'S':
	    special = 1;
	    break;
	case 'T':
	    transcript = optarg;
	    break;
	case 'V':
	    printf( "%s\n", version );
	    exit( 0 );
	case 'v':
	    verbose = 1;
	    logger = output;
	    break;
	/* diff options */
	case 'b': case 'i': case 't': case 'w':
	case 'c': case 'e': case 'f': case 'n':
	case 's':
	    if ( ( diffargv = (char **)realloc( diffargv, ( sizeof( *diffargv )
		    + ( 2 * sizeof( char * ) ) ) ) ) == NULL ) {
		perror( "malloc" );
		exit( -1 );
	    }
	    sprintf( opt, "-%c", c );
	    if ( ( diffargv[ diffargc++ ] = strdup( opt ) ) == NULL ) {
		perror( "strdup" );
		exit( -1 );
	    };
	    break;
	case 'C': case 'D': 
	    if ( ( diffargv = (char **)realloc( diffargv, ( sizeof( *diffargv )
		    + ( 3 * sizeof( char * ) ) ) ) ) == NULL ) {
		perror( "malloc" );
		exit( -1 );
	    }
	    sprintf( opt, "-%c", c );
	    if ( ( diffargv[ diffargc++ ] = strdup( opt ) ) == NULL ) {
		perror( "strdup" );
		exit( -1 );
	    };
	    diffargv[ diffargc++ ] = optarg;
	    break;
	case 'X':
	    if ( ( tac = argcargv( opt, &argcargv ) ) < 0 ) {
		err++;
	    }
	    if ( ( diffargv = (char **)realloc( diffargv, ( sizeof( *diffargv )
		    + ( tac * sizeof( char * ) ) ) ) ) == NULL ) {
		perror( "malloc" );
		exit( -1 );
	    }
	    for ( i = 0; i < tac; i++ ) {
		diffargv[ diffargc++ ] = argcargv[ i ];
	    }
	    break;
	case '?':
	    err++;
	    break;
	default:
	    err++;
	    break;
	}
    }

    if ( ( ( transcript == NULL ) && ( !special ) )
	    || ( ( special ) && ( transcript != NULL ) )
	    || ( host == NULL ) ) {
	err++;
    }

    if ( err || ( argc - optind != 1 ) ) {
	fprintf( stderr, "usage: %s ", argv[ 0 ] );
	fprintf( stderr, "[ -T transcript | -S ] " );
	fprintf( stderr, "[ -h host ] [ -p port ] [ -vV ] [ diff options ] " );
	fprintf( stderr, "[ -X \"unsupported diff options\" ] " );
	fprintf( stderr, "file\n" );
	exit( -1 );
    }
    file = argv[ optind ];

    if( ( sn = connectsn( host, port )  ) == NULL ) {
	fprintf( stderr, "%s: %d connection failed.\n", host, port );
	exit( -1 );
    }

    /* create path description */
    if ( special ) {
	if ( snprintf( pathdesc, ( MAXPATHLEN * 2 ), "SPECIAL %s",
		file ) >= ( MAXPATHLEN * 2 ) ) {
	    fprintf( stderr, "RETR SPECIAL %s: path description too long\n",
		    file );
	}
    } else {
	if ( snprintf( pathdesc, ( MAXPATHLEN * 2 ), "FILE %s %s",
		transcript, file ) >= ( MAXPATHLEN * 2 ) ) {
	    fprintf( stderr, "RETR FILE %s %s: path description too long\n",
		    transcript, file );
	}
    }

    if ( ( temppath = retr( sn, pathdesc, file, location, NULL ) )
	    == NULL ) {
	fprintf( stderr, "%s: retr failed\n", file );
	exit( -1 );
    }

    if ( ( closesn( sn ) ) != 0 ) {
	fprintf( stderr, "can not close sn\n" );
	exit( -1 );
    }

    if ( ( fd = open( temppath, O_RDONLY ) ) < 0 ) {
	perror( temppath );
	exit( -1 );
    } 
    if ( unlink( temppath ) != 0 ) {
	perror( temppath );
	exit( -1 );
    }
    if ( dup2( fd, 0 ) < 0 ) {
	perror( temppath );
	exit( -1 );
    }
    if ( ( diffargv = (char **)realloc( diffargv, ( sizeof( *diffargv )
	    + ( 4 * sizeof( char * ) ) ) ) ) == NULL ) {
	perror( "malloc" );
	exit( -1 );
    }
    diffargv[ diffargc++ ] = "--";
    diffargv[ diffargc++ ] = "-";
    diffargv[ diffargc++ ] = file; 
    diffargv[ diffargc++ ] = NULL;

    execve( diff, diffargv, envp );

    perror( diff );
    printf( "DIE DIE DIE\n" );
    exit( -1 );
}

