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
#include "connect.h"
#include "pathcmp.h"

#define MIN(X, Y) ((X) < (Y) ? (X) : (Y))

void		(*logger)( char * ) = NULL;
int		linenum = 0;
int		chksum = 0;
int		verbose = 0;
int		special = 0;
int		safe = 0;
int		network = 1;
char		transcript[ 2 * MAXPATHLEN ] = { 0 };
char		*prepath = NULL;
extern char	*version, *checksumlist;

int apply( FILE *f, char *parent, SNET *sn );
void output( char *string);

    static int
ischild( const char *parent, const char *child)
{
    size_t parentlen;

    if ( parent == NULL ) {
	return 1;
    } else {
	parentlen = strlen( parent );
	if( parentlen > strlen( child )) {
	    return 0;
	}
	if(( strncmp( parent, child, parentlen ) == 0 ) &&
		child[ parentlen ] == '/' ) {
	    return 1;
	} else {
	    return 0;
	}
    }
}

    void
output( char *string )
{
    printf( "<<< %s\n", string );
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
    char		pathdesc[ 2 * MAXPATHLEN ];
    char		chksum_b64[ 29 ];
    int			tac, present, len;
    char		**targv;
    char		*temppath = NULL, *command = "";
    char		fstype;
    struct stat		st;
    ACAV		*acav;

    acav = acav_alloc( );

    while ( fgets( tline, MAXPATHLEN, f ) != NULL ) {
	linenum++;

	len = strlen( tline );
        if (( tline[ len - 1 ] ) != '\n' ) {
	    fprintf( stderr, "%s: line too long\n", tline );
	    return( 1 );
	}

	tac = acav_parse( acav, tline, &targv );

	if ( tac == 1 ) {
	    strcpy( transcript, targv[ 0 ] );
	    len = strlen( transcript );
	    if ( transcript[ len - 1 ] != ':' ) { 
		fprintf( stderr, "%s: invalid transcript name\n", transcript );
		return( 1 );
	    }
	    transcript[ len - 1 ] = '\0';
	    if ( strcmp( transcript, "special.T" ) == 0 ) {
		special = 1;
	    } else {
		special = 0;
	    }
	    if ( verbose ) printf( "Command file: %s\n", transcript );
	    continue;
	}

	/* Get argument offset */
	if (( *targv[ 0 ] ==  '+' )
		|| ( *targv[ 0 ] == '-' )) {
	    command = targv[ 0 ];
	    targv++;
	    tac--;
	}

	if (( *command == '+' ) && ( !network )) {
	    goto linedone;
	}

	strcpy( path, decode( targv[ 1 ] ));

	/* Check transcript order */
	if ( prepath != NULL ) {
	    if ( pathcmp( path, prepath ) < 0 ) {
		fprintf( stderr, "%s: line %d: bad sort order\n",
			    transcript, linenum );
		return( 1 );
	    }
	    free( prepath );
	}
	if (( prepath = strdup( path )) == NULL ) {
	    fprintf( stderr, "strdup failed!\n" );
	    return( 1 );
	}
	    


	/* Do type check on local file */
	if ( lstat( path, &st ) ==  0 ) {
	    fstype = t_convert ((int)( S_IFMT & st.st_mode ));
	    present = 1;
	} else if ( errno == ENOENT ) { 
	    present = 0;
	} else {
	    perror( path );
	    return( 1 );
	}

	if ( *command == '-'
		|| ( present && fstype != *targv[ 0 ] )) {
	    if ( fstype == 'd' ) {

		/* Recurse */
		if ( apply( f, path, sn ) != 0 ) {
		    return( 1 );
		}

		/* Directory is now empty */
		if ( rmdir( path ) != 0 ) {
		    perror( path );
		    return( 1 );	
		}
	    } else {
		if ( unlink( path ) != 0 ) {
		    perror( path );
		    return( 1 );
		}
	    }
	    if ( verbose ) printf( "%s deleted\n", path );
	    present = 0;

	    if ( *command == '-' ) {
		goto linedone;
	    }
	}

	/* DOWNLOAD */
	if ( *command == '+' ) {
	    strcpy( chksum_b64, targv[ 7 ] );

	    if ( special ) {
		sprintf( pathdesc, "SPECIAL %s", path );
	    } else {
		sprintf( pathdesc, "FILE %s %s", transcript, path );
	    }

	    if (( temppath = retr( sn, pathdesc, path, NULL, chksum_b64 ))
		    == NULL ) {
		perror( "download" );
		return( 1 );
	    }

	    /* DO LSTAT ON NEW FILE */
	    if ( lstat( temppath, &st ) !=  0 ) {
		perror( temppath );
		return( 1 );
	    }
	    fstype = t_convert((int)( S_IFMT & st.st_mode ));
	    present = 1;

	    /* Update temp file*/
	    if ( update( temppath, present, st, tac, targv ) != 0 ) {
		perror( "update" );
		return( 1 );
	    }
	    if ( rename( temppath, path ) != 0 ) {
		perror( temppath );
		return( 1 );
	    }
	    free( temppath );
	    goto linedone;

	}

	/* UPDATE */
	if ( update( path, present, st, tac, targv ) != 0 ) {
	    perror( "update" );
	    return( 1 );
	}

linedone:
	if ( !ischild( parent, path )) {
	    goto done;
	}
    }
done:
    acav_free( acav ); 
    return( 0 );
}

    int
main( int argc, char **argv )
{
    int			c, port = htons( 6662 ), err = 0;
    extern int          optind;
    FILE		*f; 
    char		*host = _RADMIND_HOST;
    struct servent	*se;
    SNET		*sn;

    while (( c = getopt ( argc, argv, "c:h:np:sVv" )) != EOF ) {
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
	    network = 0;
	    break;
	case 'p':
	    if (( port = htons ( atoi( optarg ))) == 0 ) {
		if (( se = getservbyname( optarg, "tcp" )) == NULL ) {
		    fprintf( stderr, "%s: service unkown\n", optarg );
		    exit( 1 );
		}
		port = se->s_port;
	    }
	    break;
	case 's':
	    safe = 1;
	    break;
	case 'V':
	    printf( "%s\n", version );
	    printf( "%s\n", checksumlist );
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

    if ((( host == NULL ) && ( network ))
	    || (( host != NULL ) && ( !network ))) {
	err++;
    }

    if ( argc - optind == 0 ) {
	f = stdin; 
    } else if ( argc - optind == 1 ) {
	if (( f = fopen( argv[ optind ], "r" )) == NULL ) { 
	    perror( argv[ 1 ] );
	    goto error0;
	}
    } else {
	err++;
    }

    if ( err ) {
	fprintf( stderr, "usage: lapply [ -nsvV ] " );
	fprintf( stderr, "[ -c checksum ] [ -h host ] [ -p port ] " );
	fprintf( stderr, "[ appliable-transcript ]\n" );
	exit( 1 );
    }

    if ( network ) {
	if(( sn = connectsn( host, port )  ) == NULL ) {
	    fprintf( stderr, "%s:%d connection failed.\n", host, port );
	    goto error0;
	}
    } else {
	if ( verbose ) printf( "No network connection\n" );
    }

    if ( apply( f, NULL, sn ) != 0 ) {
	goto error2;
    }
    
    if ( fclose( f ) != 0 ) {
	perror( argv[ optind ] );
	goto error1;
    }

    if ( network ) {
	if (( closesn( sn )) !=0 ) {
	    fprintf( stderr, "can not close sn\n" );
	    goto error0;
	}
    }

    exit( 0 );

error2:
    if ( fclose( f ) != 0 ) {
	perror( argv[ optind ] );
	exit( 1 );
    }

error1:
    if ( network ) {
	if (( closesn( sn )) !=0 ) {
	    fprintf( stderr, "can not close sn\n" );
	    exit( 1 );
	}
    }

error0:
    exit( 1 );
}
