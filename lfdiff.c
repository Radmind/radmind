#include <stdlib.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

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
 *      2       System error
 */

    int
main( int argc, char **argv, char **envp )
{
    int			c, i, port = htons( 6662 ), err = 0, special = 0, tac = 0;
    int			fd;
    extern int          optind;
    extern char		*version;
    char		*host = NULL;
    char		*temppath = NULL;
    char		*transcript = NULL;
    char		*diffopts = NULL;
    char		*file = NULL;
    char		**diffargv;
    char		**targv;
    char 		pathdesc[ 2 * MAXPATHLEN ];
    struct servent	*se;
    SNET		*sn;

    while ( ( c = getopt ( argc, argv, "d:h:p:sT:Vv" ) ) != EOF ) {
	switch( c ) {
	case 'd':
	    diffopts = optarg;
	    break;
	case 'h':
	    host = optarg;
	    break;
	case 'p':
	    if ( ( port = htons ( atoi( optarg ) ) ) == 0 ) {
		if ( ( se = getservbyname( optarg, "tcp" ) ) == NULL ) {
		    fprintf( stderr, "%s: service unkown\n", optarg );
		    exit( 2 );
		}
		port = se->s_port;
	    }
	    break;
	case 's':
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
	fprintf( stderr, "[ -d \"diff opts\" ] [ -T transcript | -s ] " );
	fprintf( stderr, "[ -h host ] [ -p port ] [ -vV ] " );
	fprintf( stderr, "file\n" );
	exit( 2 );
    }
    file = argv[ optind ];

    if( ( sn = connectsn( host, port )  ) == NULL ) {
	fprintf( stderr, "%s: %d connection failed.\n", host, port );
	exit( 2 );
    }
    if ( verbose ) printf( "\n" );

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

    if ( verbose ) printf( "*** Retrieving file: %s\n", file); 
    if ( ( temppath = retr( sn, pathdesc, file, location, NULL ) )
	    == NULL ) {
	fprintf( stderr, "%s: retr failed\n", file );
	exit( 1 );
    }
    if ( verbose ) printf( "*** %s written to %s\n", file, location );

    if ( verbose ) printf( "\n" );
    if ( ( closesn( sn ) ) !=0 ) {
	fprintf( stderr, "can not close sn\n" );
	exit( 1 );
    }

    if ( ( fd = open( temppath, O_RDONLY ) ) < 0 ) {
	perror( temppath );
    } 
    if ( unlink( temppath ) != 0 ) {
	perror( temppath );
	exit( 1 );
    }
    if ( dup2( fd, 0 ) < 0 ) {
	perror( temppath );
	exit( 1 );
    }
    /* create argv to pass to diff */
    if ( diffopts != NULL ) {
	tac = acav_parse( NULL, diffopts, &targv );
    }
    if ( ( diffargv = malloc( ( tac + 5 ) * sizeof( char * ) ) ) == NULL ) {
	perror( "malloc" );
	exit( 1 );
    }
    diffargv[ 0 ] = diff;
    for ( i = 0; i < tac; i++ ) {
	diffargv[ i + 1 ] = (char *)targv[ i ];
    }
    diffargv[ tac + 1 ] = "--";
    diffargv[ tac + 2 ] = "-";
    diffargv[ tac + 3 ] = file; 
    diffargv[ tac + 4 ] = NULL;

    execve( diff, diffargv, envp );

    perror( diff );
    printf( "DIE DIE DIE\n" );
    exit( 1 );
}

