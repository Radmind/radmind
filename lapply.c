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

#define MIN(X, Y) ((X) < (Y) ? (X) : (Y))

void		(*logger)( char * ) = NULL;
struct timeval 	timeout = { 10 * 60, 0 };
int		linenum = 0;
int		verbose = 0;

    static int
ischild( const char *parent, const char *child)
{
    int parentlen;

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

    int 
apply( FILE *f, char *parent, SNET *sn )
{
    char		tline[ 2 * MAXPATHLEN ];
    char		transcript[ 2 * MAXPATHLEN ];
    char		chksum_b64[ 29 ];
    int			tac, present, len;
    char		**targv, *path, *command = "";
    char		fstype;
    struct stat		st;
    mode_t	   	mode;
    struct utimbuf	times;
    uid_t		uid;
    gid_t		gid;
    dev_t		dev;

    while ( fgets( tline, MAXPATHLEN, f ) != NULL ) {
	linenum++;

	len = strlen( tline );
        if (( tline[ len - 1 ] ) != '\n' ) {
	    fprintf( stderr, "%s: line too long\n", tline );
	    return( 1 );
	}

	tac = argcargv( tline, &targv );

	if ( tac == 1 ) {
	    if ( verbose ) printf( "Command file: %s\n", targv[ 0 ] );
	    strcpy( transcript, targv[ 0 ] );
	} else {

	    /* Get argument offset */
	    if ( ( *targv[ 0 ] == '+' ) || ( *targv[ 0 ] == '-' ) ) {
		command = targv[ 0 ];
		targv++;
		tac--;
	    }

	    path = decode( targv[ 1 ] );

	    /* Do type check on local file */
	    if ( lstat( path, &st ) ==  0 ) {
	        fstype = t_convert ( S_IFMT & st.st_mode );
		present = 1;
	    } else if ( errno != ENOENT ) { 
		perror( path );
		return( 1 );
	    } else {
		present = 0;
	    }

	    if ( present && ( *command == '-' ||  fstype != *targv[ 0 ]
		    || fstype == 'l' ) ) {

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
		    present = 0;
		} else {
		    if ( unlink( path ) != 0 ) {
			perror( path );
			return( 1 );
		    }
		    present = 0;
		}
		if ( verbose ) printf( "%s deleted\n", path );
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
		fstype = t_convert ( S_IFMT & st.st_mode );
		present = 1;
	    }

	    if ( *command != '-' ) {

		/* UPDATE */
		switch ( *targv[ 0 ] ) {
		case 'f':
		    if ( tac != 8 ) {
			// IS THIS OKAY FOR ERROR
			printf( "%s: incorrect number of arguments\n", tline );
			return( 1 );
		    }
		    mode = strtol( targv[ 2 ], (char **)NULL, 8 );
		    uid = atoi( targv[ 3 ] );
		    gid = atoi( targv[ 4 ] );
		    times.modtime = atoi( targv[ 5 ] );
		    if( mode != st.st_mode ) {
			if ( chmod( path, mode ) != 0 ) {
			    perror( path );
			    return( 1 );
			}
			if ( verbose ) printf( "    Updating %s mode\n", path );
		    }
		    if( uid != st.st_uid  || gid != st.st_gid ) {
			if ( lchown( path, uid, gid ) != 0 ) {
			    perror( path );
			    return( 1 );
			}
			if ( uid != st.st_uid ) {
			    if ( verbose ) printf( "    Updating %s uid\n", path );
			}
			if ( gid != st.st_gid ) {
			    if ( verbose ) printf( "    Updating %s gid\n", path );
			}
		    }

		    if( times.modtime != st.st_mtime ) {
			
			/*Is this what I should to for access time? */

			times.actime = st.st_atime;
			if ( utime( path, &times ) != 0 ) {
			    perror( path );
			    return( 1 );
			}
			if ( verbose ) printf( "    Updating %s time\n", path ); 
		    }
		    break;

		case 'd':

		    if ( tac != 5 ) {
			fprintf( stderr,
			    "%s: incorrect number of arguments\n", tline );
			return( 1 );
		    }

		    mode = strtol( targv[ 2 ], (char **)NULL, 8 );
		    uid = atoi( targv[ 3 ] );
		    gid = atoi( targv[ 4 ] );

		    if ( !present ) {
			mode = strtol( targv[ 2 ], (char**)NULL, 8 );
			if ( mkdir( path, mode ) != 0 ) {
			    perror( path );
			    return( 1 );
			}
			if ( lstat( path, &st ) != 0 ) {
			    perror( path );
			    return( 1 );
			}
			present = 1;
			if ( verbose ) printf( "    %s created\n", path );
		    }

		    /* check mode */
		    if( mode != st.st_mode ) {
			if ( chmod( path, mode ) != 0 ) {
			    perror( path );
			    return( 1 );
			}
			if ( verbose ) printf( "    Updating %s mode\n", path );
		    }

		    /* check uid & gid */
		    if( uid != st.st_uid  || gid != st.st_gid ) {
			if ( lchown( path, uid, gid ) != 0 ) {
			    perror( path );
			    return( 1 );
			}
			if ( uid != st.st_uid ) {
			    if ( verbose ) printf( "    Updating %s uid\n", path );
			}
			if ( gid != st.st_gid ) {
			    if ( verbose ) printf( "    Updating %s gid\n", path );
			}
		    }
		    break;

		case 'h':
		    if ( tac != 3 ) {
			fprintf( stderr,
			    "%s: incorrect number of arguments\n", tline );
			return( 1 );
		    }
		    if ( link( targv[ 2 ], path ) != 0 ) {
			perror( path );
			return( 1 );
		    }
		    if ( verbose ) printf( "    %s hard link to %s created\n",
			path, targv[ 2 ] );
		    break;

		case 'l':
		    if ( tac != 3 ) {
			fprintf( stderr,
			    "%s: incorrect number of arguments\n", tline );
			return( 1 );
		    }
		    if ( symlink( targv[ 2 ] , path ) != 0 ) {
			perror( path );
			return( 1 );
		    }
		    if ( verbose ) printf( "    %s symbolic link to %s created\n",
			path, targv[ 2 ] );
		    break;

		case 'p':
		    if ( tac != 5 ) { 
			fprintf( stderr,
			    "%s: incorrect number of arguments\n", tline );
			return( 1 );
		    }
		    mode = strtol( targv[ 2 ], (char **)NULL, 8 );
		    uid = atoi( targv[ 3 ] );
		    gid = atoi( targv[ 4 ] );
		    if ( !present ) {
			fprintf( stderr, "%s: Named pipe not present\n", path );
			return( 1 );
		    }
		    /* check mode */
		    if( mode != st.st_mode ) {
			if ( chmod( path, mode ) != 0 ) {
			    perror( path );
			    return( 1 );
			}
			if ( verbose ) printf( "    Updating %s mode\n", path );
		    }
		    /* check uid & gid */
		    if( uid != st.st_uid  || gid != st.st_gid ) {
			if ( lchown( path, uid, gid ) != 0 ) {
			    perror( path );
			    return( 1 );
			}
			if ( uid != st.st_uid ) {
			    if ( verbose ) printf( "    Updating %s uid\n", path );
			}
			if ( gid != st.st_gid ) {
			    if ( verbose ) printf( "    Updating %s gid\n", path );
			}
		    }
		    break;

		case 'b':
		case 'c':
		    if ( tac != 7 ) {
			fprintf( stderr,
			    "%s: incorrect number of arguments\n", tline );
			return( 1 );
		    }

		    if ( present && ( ( minor( st.st_rdev )
			    != atoi( targv[ 6 ] ) ) || ( major( st.st_rdev )
			    != atoi( targv[ 5 ] ) ) ) ) {
			if ( unlink( path ) != 0 ) {
			    perror( path );
			    return( 1 );
			}
			present = 0;
		    }

		    mode = strtol( targv[ 2 ], (char **)NULL, 8 );
		    if( *targv[ 0 ] == 'b' ) {
			mode = mode | S_IFBLK;
		    } else {
			mode = mode | S_IFCHR;
		    }
		    uid = atoi( targv[ 3 ] );
		    gid = atoi( targv[ 4 ] );
		    if ( !present ) {
			if ( ( dev = makedev( atoi( targv[ 5 ] ),
				atoi( targv[ 6 ] ) ) ) == NODEV ) {
			    perror( path );
			    return( 1 );
			}
			if ( mknod( path, mode, dev ) != 0 ) {
			    perror( path );
			    return( 1 );
			}
			if ( lstat( path, &st ) !=  0 ) {
			    perror( path );
			    return( 1 );
			}
			present = 1;
			if ( verbose ) printf( "%s created\n", path );
		    }
		    /* check mode */
		    if( mode != st.st_mode ) {
			if ( chmod( path, mode ) != 0 ) {
			    perror( path );
			    return( 1 );
			}
			if ( verbose ) printf( "Updating: %s mode\n", path );
		    }
		    /* check uid & gid */
		    if( uid != st.st_uid  || gid != st.st_gid ) {
			if ( lchown( path, uid, gid ) != 0 ) {
			    perror( path );
			    return( 1 );
			}
			if ( uid != st.st_uid ) {
			    if ( verbose ) printf( "Updating %s uid\n", path );
			}
			if ( gid != st.st_gid ) {
			    if ( verbose ) printf( "Updating %s gid\n", path );
			}
		    }
		    break;
		default :
		    printf( "%d: Unkown type %s\n", linenum, targv[ 0 ] );
		    return( 1 );
		} // End of defualt switch
	    }

	    /* check for child */

	    if ( ! ischild( parent, path ) ) {
		return( 0 );
	    }
	}
    }

    return( 0 );
}

    int
main( int argc, char **argv )
{
    int			i, c, s, port = htons( 6662 ), err = 0, network = 1;
    FILE		*f; 
    char		*host = "rearwindow.rsug.itd.umich.edu";
    char		*line;
    struct servent	*se;
    char		*version = "1.0";
    struct hostent	*he;
    struct sockaddr_in	sin;
    SNET		*sn;
    struct timeval	tv;

    while ( ( c = getopt ( argc, argv, "h:np:Vv" ) ) != EOF ) {
	switch( c ) {
	case 'h':
	    host = optarg;
	    break;
	case 'n':
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

    if ( ( f = fopen( argv[ 1 ], "r" ) ) == NULL ) { 
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
