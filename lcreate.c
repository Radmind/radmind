/*
 * STOR
 * C: STOR <path-decription> "\r\n"
 * S: 350 Storing file "\r\n"
 * C: <size> "\r\n"
 * C: <size bytes of file data>
 * C: ".\r\n"
 * S: 250 File stored "\r\n"
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <strings.h>

#include "snet.h"
#include "argcargv.h"

void		(*logger)( char * ) = NULL;
int		verbose = 0;

    void
v_logger( char *line )
{
    fprintf( stderr, "<<< %s\n", line );
    return;
}



    static int
store_file( int fd, SNET *sn, char *filename, char *transcript ) 
{
    struct stat 	st;
    struct timeval 	tv = { 10, 0 };
    unsigned char	buf[ 8192 ];
    unsigned int	rr;
    char		*line;

    if ( fstat( fd, &st) < 0 ) {
	perror( filename );
	return( -1 );
    }

    /* STOR "TRANSCRIPT" <transcript-name>  "\r\n" */
    if ( filename == NULL ) {
	if ( snet_writef( sn,
		"STOR TRANSCRIPT %s\r\n", transcript ) == NULL ) {
	    perror( "snet_writef" );
	    return( -1 );
	}
	if ( verbose ) {
	    fprintf( stderr, ">>> STOR TRANSCRIPT %s\n", transcript );
	}
    } else {  /* STOR "FILE" <transcript-name> <path> "\r\n" */
	if ( snet_writef( sn,
		"STOR FILE %s %s\r\n", transcript, filename ) == NULL ) {
	    perror( "snet_writef" );
	    return( -1 );
	}
	if ( verbose ) {
	    fprintf( stderr, ">>> STOR FILE %s %s\n", transcript, filename );
	}
    }

    if (( line = snet_getline_multi( sn, logger, &tv )) == NULL ) {
	perror( "snet_getline_multi" );
	return( -1 );
    }
    if ( *line != '3' ) {
	fprintf( stderr, "%s\n", line );
	return( -1 );
    }

    if ( snet_writef( sn, "%d\r\n", st.st_size ) == NULL ) {
	perror( "snet_writef" );
	return( -1 );
    }
    if ( verbose ) fprintf( stderr, ">>> %d\n", (int)st.st_size );
    
    while (( rr = read( fd, buf, sizeof( buf ))) > 0 ) {
	if ( verbose ) { fputs( "...", stderr ); fflush( stderr ); }
	tv.tv_sec = 10;
	tv.tv_usec = 0;
	if ( snet_write( sn, buf, (int)rr, &tv ) != rr ) {
	    perror( "snet_write" );
	    return( -1 );
	}
    }

    if ( snet_writef( sn, ".\r\n" ) == NULL ) {
	perror( "snet_writef" );
	return( -1 );
    }
    if ( verbose ) fputs( "\n>>> .\n", stderr );

    tv.tv_sec = 10;
    tv.tv_usec = 0;
    if (( line = snet_getline_multi( sn, logger, &tv )) == NULL ) {
	perror( "snet_getline_multi" );
	return( -1 );
    }
    if ( *line != '2' ) {
	fprintf( stderr, "%s\n", line );
	return( -1 );
    }

    return( 0 );

}

    int
main( int argc, char **argv )
{
    int			i, s, c, err = 0, port = htons(6662), tac, fd, fdt;
    int			network = 1, exitcode = 0, len, rc;
    extern int		optind;
    struct hostent	*he;
    struct servent	*se;
    struct timeval	tv; 
    struct sockaddr_in	sin;
    SNET          	*sn;
    char		*tname = NULL, *host = "rsug.itd.umich.edu"; 
    char		*p,*line, tline[ 2 * MAXPATHLEN ];
    char		**targv;
    extern char		*optarg;
    FILE		*fdiff; 

    while (( c = getopt( argc, argv, "h:np:t:v" )) != EOF ) {
	switch( c ) {
	case 'h':
	    host = optarg; 
	    break;
	case 'n':
	    network = 0;
	    break;

	case 'p':
	    if (( port = htons( atoi( optarg ))) == 0 ) {
		if (( se = getservbyname( optarg, "tcp" )) == NULL ) {
		    fprintf( stderr, "%s: service unknown\n", optarg );
		    exit( 1 );
		}
		port = se->s_port;
	    }
	    break;

	case 't':
	    tname = optarg;
	    break;
	case 'v':
	    verbose = 1;
	    logger = v_logger;
	    break;
	case '?':
	    err++;
	    break;
	default:
	    err++;
	    break;
	}
    }

    if ( err || ( argc - optind != 1 ))   {
	fprintf( stderr, "usage: lcreate [ -nv ] " );
	fprintf( stderr, "[ -h host ] [-p port ] [ -t stored-name ] ");
	fprintf( stderr, "difference-transcript\n" );
	exit( 1 );
    }

    if (( fd = open( argv[ optind ], O_RDONLY, 0 )) < 0 ) {
	perror( argv[ optind ] );
	exit( 1 );
    }

    if ( network ) {
	/* no name given on command line, so make a "default" name */
	if ( tname == NULL ) {
	    tname = argv[ optind ];
	    /* strip leading "/"s */
	    if (( p = strrchr( tname, '/' )) != NULL ) {
		tname = ++p;
	    }

	    /* strip trailing ".T" only */
	    if ((( p = strrchr( tname, '.' )) != NULL ) &&
		    ( strcmp( p, ".T") == 0 )) {
		*p = '\0';
	    }
	}

	if (( he = gethostbyname( host )) == NULL ) {
	    fprintf( stderr, "%s: host unknown\n", host );
	    exit( 1 );
	}

	for ( i = 0; he->h_addr_list[ i ] != NULL; i++ ) {
	    if (( s = socket( PF_INET, SOCK_STREAM, NULL)) < 0 ) {
		perror( "socket failed" );
		exit( 1 );
	    }
	    memset( &sin, 0, sizeof( struct sockaddr_in ));
	    sin.sin_family = AF_INET;
	    sin.sin_port =  port;
	    memcpy( &sin.sin_addr.s_addr, he->h_addr_list[ i ],
		    (unsigned int)he->h_length );
	    fprintf( stderr, "trying %s... ", inet_ntoa( *(struct
		    in_addr *)he->h_addr_list[ i ] ));
	    if ( connect( s, (struct sockaddr *)&sin,
		    sizeof( struct sockaddr_in )) != 0 ) {
		perror( "connect" );
		(void)close( s );
		continue;
	    }
	    fprintf( stderr, "success!\n" );

	    if (( sn = snet_attach( s, 1024 * 1024 )) == NULL ) {
		perror( "snet_attach failed");
		continue;
	    }

	    tv.tv_sec = 10;
	    tv.tv_usec = 0;
	    if (( line = snet_getline_multi( sn, logger, &tv )) == NULL ) {
		perror( "snet_getline_multi" );
		if ( snet_close( sn ) != 0 ) {
		    perror( "snet_close" );
		}
		continue;
	    }

	    if ( *line != '2' ) {
		fprintf( stderr, "%s\n", line );
		if ( snet_close( sn ) != 0 ) {
		    perror( "snet_close" );
		}
		continue;
	    }
	    break;
	}

	if ( he->h_addr_list[ i ] == NULL ) {
	    fprintf( stderr, "connection failed\n" );
	    exit( 1 );
	}

	if ( store_file( fd, sn, NULL, tname ) != 0 ) {
	    fprintf( stderr, "failed to store transcript \"%s\"\n", tname );
	    exitcode = 1;
	    (void)close( fd );
	    goto done;
	}

	/* lseek back to the beginning */
	if ( lseek( fd, 0L, SEEK_SET) != 0 ) {
	    perror( "lseek" );
	    (void)close( fd );
	    goto done;
	}
    }

    if (( fdiff = fdopen( fd, "r" )) == NULL ) {
	perror( "fdopen" );
	exit( 1 );
    }

    while ( fgets( tline, MAXPATHLEN, fdiff ) != NULL ) {
	len = strlen( tline );
	if (( tline[ len - 1 ] ) != '\n' ) {
	    fprintf( stderr, "%s: line too long\n", tline );
	    exitcode = 1;
	    break;
	}
	tac = argcargv( tline, &targv );
	if ( *targv[ 0 ] == 'f' && tac >= 2 ) {
	    if ( !network ) {
		if ( access( targv[ 1 ],  R_OK ) < 0 ) {
		    perror( targv[ 1 ] );
		    exitcode = 1;
		}
	    } else {
		if (( fdt = open( targv[ 1 ], O_RDONLY, 0 )) < 0 ) {
		    perror( targv[ 1 ] );
		    exitcode = 1;
		    break;
		} 

		rc = store_file( fdt, sn, targv[ 1 ], tname ); 
		(void)close( fdt ); 
		if ( rc != 0 ) {
		    fprintf( stderr, "failed to store file %s\n", targv[ 1 ] );
		    exitcode = 1;
		    break;
		}

	    }
	}
    }
    (void)fclose( fdiff );

done:
    if ( network ) {
	if ( snet_writef( sn, "QUIT\r\n" ) == NULL ) {
	    perror( "snet_writef" );
	    exit( 1 );
	}
	if ( verbose ) fputs( ">>> QUIT\n", stderr );

	if (( line = snet_getline_multi( sn, logger, &tv )) == NULL ) {
	    perror( "snet_getline_multi" );
	    exit( 1 );
	}
	if ( *line != '2' ) {
	    fprintf( stderr, "%s\n", line );
	}
    
	if ( snet_close( sn ) != 0 ) {
	    perror( "snet_close" );
	    exit( 1 );
	} 
    }

    exit( exitcode );
}
