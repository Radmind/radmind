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

    int
main( int argc, char **argv )
{
    int			i, s, c, err = 0, port = 6662, tac, fd, fdt;
    int			verbose = 0, network = 1, exitcode = 0;
    extern int		errno, optind;
    struct hostent	*he;
    struct stat		st;
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
	    fprintf( stderr, "n option selected\n" );
	    break;
	case 'p':
	    port = atoi( optarg );
	    break;
	case 't':
	    tname = optarg;
	    fprintf( stderr, "tname opt: %s\n", tname );
	    break;
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

    if ( err || ( argc - optind != 1 ))   {
	fprintf( stderr, "usage: lcreate [ -nv ] " );
	fprintf( stderr, "[ -h host ] [-p port ] [ -t stored name ] diff\n" );
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
	    if ((( p = strrchr( tname, '.' )) != NULL )
		    && ( strcmp( p, ".T") == 0 )) {
		*p = '\0';
	    }
	}

	if (( he = gethostbyname( host )) == NULL ) {
	    fprintf( stderr, "An error occured while looking up the host.\n" );
	    exit( 1 );
	}

	for ( i = 0; he->h_addr_list[ i ] != NULL; i++ ) {
	    if (( s = socket( PF_INET, SOCK_STREAM, NULL)) < 0 ) {
		perror( "socket failed" );
		exit( 1 );
	    }
	    memset( &sin, 0, sizeof( struct sockaddr_in ));
	    sin.sin_family = AF_INET;
	    sin.sin_port = htons( port );
	    memcpy( &sin.sin_addr.s_addr, he->h_addr_list[ i ],
		    he->h_length );
	    fprintf( stderr, "trying %s... ", inet_ntoa( *(struct
		    in_addr *)he->h_addr_list[ i ] ));
	    if ( connect( s, (struct sockaddr *)&sin,
		    sizeof( struct sockaddr_in )) != 0 ) {
		perror( "connect failed" );
		close( s );
		continue;
	    }
	    fprintf( stderr, "success!\n" );
	    break;
	}

	if ( he->h_addr_list[ i ] == NULL ) {
	    perror( "no connects established" );
	    exit( 1 );
	}

	if (( sn = snet_attach( s, 1024 * 1024 )) == NULL ) {
	    perror( "snet_attach failed");
	    exit( 1 );
	}

	tv.tv_sec = 10;
	tv.tv_usec = 0;

	if (( line = snet_getline_multi( sn, NULL, &tv )) == NULL ) {
	    perror( "banner not happy" );
	    exit( 1 );
	}

	if ( *line != '2' ) {
	    fprintf( stderr, "not a 2\n" );
	}
	/* upload transcript */
	/* lseek back to the beginning */
    }


    if (( fdiff = fdopen( fd, "r" )) == NULL ) {
	perror( "fdopen" );
	exit( 1 );
    }

    while ( fgets( tline, MAXPATHLEN, fdiff ) != NULL ) {
	tac = argcargv( tline, &targv );
	if ( *targv[ 0 ] == 'f' && tac >= 2 ) {
	    fprintf( stderr, "%s is a file!\n", targv[ 1 ] );
	    if ( !network ) {
		if ( access( targv[ 1 ],  R_OK ) < 0 ) {
		    perror( targv[ 1 ] );
		    exitcode = 1;
		}
	    } else {
		if (( fdt = open( targv[ 1 ], O_RDONLY, 0 )) < 0 ) {
		    perror( targv[ 1 ] );
		    exitcode = 1;
		    goto done;
		} else {
		    if ( fstat( fdt, &st) < 0 ) {
			perror( targv[ 1 ] );
			exitcode = 1;
			goto done;
		    }
		}
	    /* open */
	    /* fstat */
	    /* snetwritef info to server */
	    /* read */
	    /* snetwrite */
	    }
	}

    }


#ifdef notdef
    if ( net_writef( n, "auth anonymous canna\r\n" ) == NULL ) {
	perror( "net_writef" );
	exit( 1 );
    } 

    if (( line = net_getline_multi( n, flag, &tv )) == NULL ) {
	perror( "auth not happy" );
	exit( 1 );
    }

    if ( *line != '2' ) {
	fprintf( stderr, "not a 2\n" );
    }
#endif notdef

done:

    fclose( fdiff );

    if ( network ) {
	if ( snet_writef( sn, "quit\r\n" ) == NULL ) {
	    perror( "snet_writef" );
	    exit( 1 );
	}

	if (( line = snet_getline_multi( sn, NULL, &tv )) == NULL ) {
	    perror( "quit not happy" );
	    exit( 1 );
	}

	if ( *line != '2' ) {
	    fprintf( stderr, "not a 2\n" );
	}
    
	if ( snet_close( sn ) != 0 ) {
	    perror( "snet_close" );
	    exit( 1 );
	} 
    }

    exit( exitcode );
}
