#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <sha.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/mkdev.h>
#include <sys/ddi.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <strings.h>
#include <utime.h>
#include <errno.h>

#include "snet.h"
#include "argcargv.h"
#include "code.h"
#include "base64.h"
#include "convert.h"
#include "download.h"
#include "update.h"

#define MIN(X, Y) ((X) < (Y) ? (X) : (Y))


void		(*logger)( char * ) = NULL;
struct timeval 	timeout = { 10 * 60, 0 };
int		linenum = 0;
int		chksum = 0;
int		verbose = 0;
char		transcript[ 2 * MAXPATHLEN ];

int apply( FILE *f, char *parent, SNET *sn );
void output( char* string);

    static int
ischild( const char *parent, const char *child)
{
    size_t parentlen;

    if ( parent == NULL ) {
	return 1;
    } else {
	parentlen = strlen( parent );
	if( parentlen > strlen( child ) ) {
	    return 0;
	}
	if( ( strncmp( parent, child, parentlen ) == 0 ) &&
		child[ parentlen + 1 ] == '/' ) {
	    return 1;
	} else {
	    return 0;
	}
    }
}

    void
output( char *string ) {
    printf( "%s\n", string );
    return;
}

/*
 * Never exit.  Must return so main can close network connection.
 * 
 * Must save parent and command pointers between recursive calls.
 */

    int 
apply( FILE *f, char *parent, SNET *sn )
{
    char		tline[ 2 * MAXPATHLEN ];
    char		path[ 2 * MAXPATHLEN ];
    char		chksum_b64[ 29 ];
    int			tac, present, len;
    char		**targv;
    char		*tempparent, *command = "", *tempcommand;
    char		fstype;
    struct stat		st;

    while ( fgets( tline, MAXPATHLEN, f ) != NULL ) {
	linenum++;

	if ( verbose ) printf( "\n%d: %s", linenum, tline );

	len = strlen( tline );
        if (( tline[ len - 1 ] ) != '\n' ) {
	    fprintf( stderr, "%s: line too long\n", tline );
	    return( 1 );
	}

	tac = argcargv( tline, &targv );

	/*
	 * What if the next line in a recursive call has the command
	 * file name?  We are going to lose this when we return.  Do
	 * we make it global?
	 */

	if ( tac == 1 ) {
	    strcpy( transcript, targv[ 0 ] );
	    len = strlen( transcript );
	    if ( ( transcript[ len - 1 ] != ':' )
		    || ( transcript[ len - 2 ] != 'T' )
		    || ( transcript[ len - 3 ] != '.' ) ) {
		fprintf( stderr, "%s: invalid transcript name\n", transcript );
		return( 1 );
	    }
	    transcript[ len - 1 ] = NULL;
	    transcript[ len - 2 ] = NULL;
	    transcript[ len - 3 ] = NULL;
	    if ( verbose ) printf( "Command file: %s\n", transcript );
	    continue;
	}

	/* Get argument offset */
	/*
	 * Why can't I have:
	 * ( *targv[ 0 ] == ( '+' || '-' ) );
	 */
	if ( ( *targv[ 0 ] ==  '+' ) || ( *targv[ 0 ] == '-' ) ) {
	    command = targv[ 0 ];
	    targv++;
	    tac--;
	}

	strcpy( path, decode( targv[ 1 ] ) );

	/* Do type check on local file */
	if ( lstat( path, &st ) ==  0 ) {
	    fstype = t_convert ( (int)( S_IFMT & st.st_mode ) );
	    present = 1;
	} else if ( errno == ENOENT ) { 
	    present = 0;
	} else {
	    perror( path );
	    return( 1 );
	}

	if ( *command == '-' || ( present && fstype != *targv[ 0 ] ) ) {
	    if ( fstype == 'd' ) {

		/* Save pointers */
		tempparent = parent;
		tempcommand = command;

		/* Recurse */
		if ( apply( f, path, sn ) != 0 ) {
		    return( 1 );
		}

		/* Restore pointers */
		parent = tempparent;
		command = tempcommand;

		/* Directory is now empty */
		if ( rmdir( path ) != 0 ) {
		    perror( path );
		    return( 1 );	
		}
		present = 0;
	    } else {
		if ( unlink( path ) != 0 ) {
		    perror( path );
		    return( 1 );
		}
		present = 0;
	    }
	    if ( verbose ) printf( "%s deleted\n", path );

	    if ( *command == '-' ) {
		goto linedone;
	    }
	}

	/* DOWNLOAD */
	if ( *command == '+' ) {
	    strcpy( chksum_b64, targv[ 7 ] );

	    if ( download( sn, transcript, path, chksum_b64 ) != 0 ) {
		perror( "download" );
		return( -1 );
	    }

	    /* DO LSTAT ON NEW FILE */
	    if ( lstat( path, &st ) !=  0 ) {
		perror( "path" );
		return ( -1 );
	    }
	    fstype = t_convert ( (int)( S_IFMT & st.st_mode ) );
	    present = 1;
	}

	/* UPDATE */
	if ( update( path, present, st, tac, targv ) != 0 ) {
	    perror( "update" );
	    return( 1 );
	}

linedone:
	if ( !ischild( parent, path ) ) {
	    return( 0 );
	}
    }
    
    return( 0 );
}

    int
main( int argc, char **argv )
{
    int			i, c, s, port = htons( 6662 ), err = 0, network = 1;
    extern int          optind;
    FILE		*f; 
    char		*host = "rearwindow.rsug.itd.umich.edu";
    char		*line;
    struct servent	*se;
    char		*version = "1.0";
    struct hostent	*he;
    struct sockaddr_in	sin;
    SNET		*sn;
    struct timeval	tv;

    while ( ( c = getopt ( argc, argv, "c:h:np:Vv" ) ) != EOF ) {
	switch( c ) {
	case 'c':
	    if ( strcasecmp( optarg, "sha1" ) != 0 ) {
		perror( optarg );
		exit( 1 );
	    }
	    chksum = 1;
	    break;
	case 'h':
	    host = optarg;
	    break;
	case 'n':
	    printf( "No network connection\n" );
	    network = 0;
	    break;
	case 'p':
	    if ( ( port = htons ( atoi( optarg ) ) ) == 0 ) {
		if ( ( se = getservbyname( optarg, "tcp" ) ) == NULL ) {
		    fprintf( stderr, "%s: service unkown\n", optarg );
		    exit( 1 );
		}
		port = se->s_port;
	    }
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

    if ( err || ( argc - optind != 1 ) ) {
	fprintf( stderr, "usage: lapply [ -nv ] " );
	fprintf( stderr, "[ -h host ] [ -port ] " );
	fprintf( stderr, "difference-transcript\n" );
	exit( 1 );
    }

    if ( network ) {
	if ( ( he = gethostbyname( host ) ) == NULL ) {
	    perror( host );
	    exit( 1 );
	}

	for ( i = 0; he->h_addr_list[ i ] != NULL; i++ ) {
	    if ( ( s = socket( PF_INET, SOCK_STREAM, NULL ) ) < 0 ) {
		perror ( host );
		exit( 1 );
	    }
	    memset( &sin, 0, sizeof( struct sockaddr_in ) );
	    sin.sin_family = AF_INET;
	    sin.sin_port = port;
	    memcpy( &sin.sin_addr.s_addr, he->h_addr_list[ i ],
		( unsigned int)he->h_length );
	    if ( verbose ) printf( "trying %s... ",
		    inet_ntoa( *( struct in_addr *)he->h_addr_list[ i ] ) );
	    if ( connect( s, ( struct sockaddr *)&sin,
		    sizeof( struct sockaddr_in ) ) != 0 ) {
		perror( "connect" );
		(void)close( s );
		continue;
	    }
	    if ( verbose ) printf( "success!\n" );

	    if ( ( sn = snet_attach( s, 1024 * 1024 ) ) == NULL ) {
		perror ( "snet_attach failed" );
		continue;
	    }

	    tv.tv_sec = 10;
	    tv.tv_usec = 0;
	    if ( ( line = snet_getline_multi( sn, logger, &tv) ) == NULL ) {
		perror( "snet_getline_multi" );
		if ( snet_close( sn ) != 0 ) {
		    perror ( "snet_close" );
		}
		continue;
	    }

	    if ( *line !='2' ) {
		fprintf( stderr, "%s\n", line);
		if ( snet_close( sn ) != 0 ) {
		    perror ( "snet_close" );
		}
		continue;
	    }
	    break;
	}

	if ( he->h_addr_list[ i ] == NULL ) {
	    perror( "connection failed" );
	    exit( 1 );
	}
    }

    if ( ( f = fopen( argv[ optind ], "r" ) ) == NULL ) { 
	perror( argv[ 1 ] );
	exit( 1 );
    }

    if ( apply( f, NULL, sn ) != 0 ) {
	fclose( f );
	exit( 1 );
    }

    if ( network ) {
	if ( snet_writef( sn, "QUIT\r\n" ) == NULL ) {
	    perror( "snet_writef" );
	    exit( 1 );
	}

	if ( ( line = snet_getline_multi( sn, logger, &tv ) ) == NULL ) {
	    perror( "snet_getline_multi" );
	    exit( 1 );
	}

	if ( *line != '2' ) {
	    perror( line );
	}

	if ( snet_close( sn ) != 0 ) {
	    perror( "snet_close" );
	    exit( 1 );
	}
    }

    exit( 0 );
}
