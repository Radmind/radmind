#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <netdb.h>
#include <snet.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>

#include "connect.h"
#include "retr.h"
#include "argcargv.h"

void output( char* string);

void			(*logger)( char * ) = NULL;
extern struct timeval	timeout;
int			verbose = 0;
int			linenum = 0;
int			chksum = 0;
char			fullpath[ MAXPATHLEN ];
char			*diff = _PATH_GNU_DIFF;
char			*location = "/tmp/lfdiff";

    void
output( char *string )
{
    printf( "<<< %s\n", string );
    return;
}

/*
 * exit codes:
 *      0       No differences were found.
 *	1	Differences were found.
 *      >1     	An error occurred. 
 */

    int
main( int argc, char **argv, char **envp )
{
    int			c, i, tac, port = htons( 6662 ), err = 0;
    int			special = 0, diffargc = 0;
    int			fd;
    extern int          optind; 
    extern char		*version;
    char		*host = _RADMIND_HOST;
    char		*transcript = NULL;
    char		*file = NULL;
    char		**diffargv;
    char		**argcargv;
    char 		pathdesc[ 2 * MAXPATHLEN ];
    char 		temppath[ 2 * MAXPATHLEN ];
    char		opt[ 3 ];
    struct servent	*se;
    SNET		*sn;

    /* create argv to pass to diff */
    if (( diffargv = (char **)malloc( 1  * sizeof( char * ))) == NULL ) {
	perror( "malloc" );
	exit( 2 );
    }
    diffargc = 0;
    diffargv[ diffargc++ ] = diff;

    while (( c = getopt ( argc, argv, "h:p:ST:VvbitwcefnC:D:sX:" ))
	    != EOF ) {
	switch( c ) {
	case 'h':
	    host = optarg;
	    break;
	case 'p':
	    if (( port = htons ( atoi( optarg ))) == 0 ) {
		if (( se = getservbyname( optarg, "tcp" )) == NULL ) {
		    fprintf( stderr, "%s: service unkown\n", optarg );
		    exit( 2 );
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
	    if (( diffargv = (char **)realloc( diffargv, ( sizeof( *diffargv )
		    + ( 2 * sizeof( char * ))))) == NULL ) {
		perror( "malloc" );
		exit( 2 );
	    }
	    if ( snprintf( opt, sizeof( opt ), "-%c", c ) > sizeof( opt )) {
		fprintf( stderr, "-%c: too large\n", c );
		exit( 2 );
	    }
	    if (( diffargv[ diffargc++ ] = strdup( opt )) == NULL ) {
		perror( "strdup" );
		exit( 2 );
	    };
	    break;
	case 'C': case 'D': 
	    if (( diffargv = (char **)realloc( diffargv, ( sizeof( *diffargv )
		    + ( 3 * sizeof( char * ))))) == NULL ) {
		perror( "malloc" );
		exit( 2 );
	    }
	    if ( snprintf( opt, sizeof( opt ), "-%c", c ) > sizeof( opt )) {
		fprintf( stderr, "-%c: too large\n", c );
		exit( 2 );
	    }
	    if (( diffargv[ diffargc++ ] = strdup( opt )) == NULL ) {
		perror( "strdup" );
		exit( 2 );
	    };
	    diffargv[ diffargc++ ] = optarg;
	    break;
	case 'X':
	    if (( tac = argcargv( opt, &argcargv )) < 0 ) {
		err++;
	    }
	    if (( diffargv = (char **)realloc( diffargv, ( sizeof( *diffargv )
		    + ( tac * sizeof( char * ))))) == NULL ) {
		perror( "malloc" );
		exit( 2 );
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

    if ((( transcript == NULL ) && ( !special ))
	    || (( special ) && ( transcript != NULL ))
	    || ( host == NULL )) {
	err++;
    }

    if ( err || ( argc - optind != 1 )) {
	fprintf( stderr, "usage: %s ", argv[ 0 ] );
	fprintf( stderr, "[ -T transcript | -S ] " );
	fprintf( stderr, "[ -h host ] [ -p port ] [ -vV ] [ diff options ] " );
	fprintf( stderr, "[ -X \"unsupported diff options\" ] " );
	fprintf( stderr, "file\n" );
	exit( 2 );
    }
    file = argv[ optind ];

    if(( sn = connectsn( host, port )  ) == NULL ) {
	fprintf( stderr, "%s: %d connection failed.\n", host, port );
	exit( 2 );
    }

    /* create path description */
    if ( special ) {
	if ( snprintf( pathdesc, ( MAXPATHLEN * 2 ), "SPECIAL %s",
		file ) >= ( MAXPATHLEN * 2 )) {
	    fprintf( stderr, "RETR SPECIAL %s: path description too long\n",
		    file );
	}
    } else {
	if ( snprintf( pathdesc, ( MAXPATHLEN * 2 ), "FILE %s %s",
		transcript, file ) >= ( MAXPATHLEN * 2 )) {
	    fprintf( stderr, "RETR FILE %s %s: path description too long\n",
		    transcript, file );
	}
    }

    if ( retr( sn, pathdesc, file, location, NULL, (char *)&temppath ) != 0 ) {
	fprintf( stderr, "%s: retr failed\n", file );
	exit( 2 );
    }

    if (( closesn( sn )) != 0 ) {
	fprintf( stderr, "can not close sn\n" );
	exit( 2 );
    }

    if (( fd = open( temppath, O_RDONLY )) < 0 ) {
	perror( temppath );
	exit( 2 );
    } 
    if ( unlink( temppath ) != 0 ) {
	perror( temppath );
	exit( 2 );
    }
    if ( dup2( fd, 0 ) < 0 ) {
	perror( temppath );
	exit( 2 );
    }
    if (( diffargv = (char **)realloc( diffargv, ( sizeof( *diffargv )
	    + ( 4 * sizeof( char * ))))) == NULL ) {
	perror( "malloc" );
	exit( 2 );
    }
    diffargv[ diffargc++ ] = "--";
    diffargv[ diffargc++ ] = "-";
    diffargv[ diffargc++ ] = file; 
    diffargv[ diffargc++ ] = NULL;

    execve( diff, diffargv, envp );

    perror( diff );
    exit( 2 );
}
