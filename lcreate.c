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
#include "code.h"
#include "connect.h"
#include "argcargv.h"

void		(*logger)( char * ) = NULL;
int		verbose = 0;
extern char	*version;

    static void
v_logger( char *line )
{
    printf( "<<< %s\n", line );
    return;
}

    static int
n_store_file( SNET *sn, char *filename, char *transcript )
{
    struct timeval	tv;
    char		*line;

    if ( snet_writef( sn,
                "STOR FILE %s %s\r\n", transcript, filename ) == NULL ) {
            perror( "snet_writef" );
            return( -1 );
    }

    if ( verbose ) {
	printf( ">>> STOR FILE %s %s\n", transcript, filename );
    }

    tv.tv_sec = 120;
    tv.tv_usec = 0;
    if (( line = snet_getline_multi( sn, logger, &tv )) == NULL ) {
        perror( "snet_getline_multi" );
        return( -1 );
    }
    if ( *line != '3' ) {
        fprintf( stderr, "%s\n", line );
        return( -1 );
    }

    if ( snet_writef( sn, "0\r\n.\r\n" ) == NULL ) {
        perror( "snet_writef" );
        return( -1 );
    }
    if ( verbose ) fputs( ">>> 0\n\n>>> .\n", stdout );

    tv.tv_sec = 120;
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

    static int
store_file( int fd, SNET *sn, char *filename, char *transcript ) 
{
    struct stat 	st;
    struct timeval 	tv;
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
	    printf( ">>> STOR TRANSCRIPT %s\n", transcript );
	}
    } else {  /* STOR "FILE" <transcript-name> <path> "\r\n" */
	if ( snet_writef( sn,
		"STOR FILE %s %s\r\n", transcript, filename ) == NULL ) {
	    perror( "snet_writef" );
	    return( -1 );
	}
	if ( verbose ) {
	    printf( ">>> STOR FILE %s %s\n", transcript, filename );
	}
    }

    tv.tv_sec = 120;
    tv.tv_usec = 0;
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
    if ( verbose ) printf( ">>> %d\n", (int)st.st_size );
    
    while (( rr = read( fd, buf, sizeof( buf ))) > 0 ) {
	if ( verbose ) { fputs( "...", stdout ); fflush( stdout ); }
	tv.tv_sec = 120;
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
    if ( verbose ) fputs( "\n>>> .\n", stdout );

    tv.tv_sec = 120;
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
    int			c, err = 0, port = htons(6662), tac, fd, fdt;
    int			network = 1, exitcode = 0, len, rc;
    int			negative = 0, tran_only = 0;
    extern int		optind;
    struct servent	*se;
    SNET          	*sn;
    char		*tname = NULL, *host = "rsug.itd.umich.edu"; 
    char		*p,*dpath, tline[ 2 * MAXPATHLEN ];
    char		**targv;
    extern char		*optarg;
    FILE		*fdiff; 

    while (( c = getopt( argc, argv, "h:nNp:t:TvV" )) != EOF ) {
	switch( c ) {
	case 'h':
	    host = optarg; 
	    break;
	case 'n':
	    network = 0;
	    break;

	case 'N':
	    negative = 1;
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

	case 'T':
	    tran_only = 1;
	    break;

	case 'v':
	    verbose = 1;
	    logger = v_logger;
	    break;
	case 'V':
	    printf( "%s\n", version );
	    exit( 0 );
	case '?':
	    err++;
	    break;
	default:
	    err++;
	    break;
	}
    }

    if ( err || ( argc - optind != 1 ))   {
	fprintf( stderr, "usage: lcreate [ -nNTvV ] " );
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
	}


	if (( sn = connectsn( host, port )) == NULL ) {
	    fprintf( stderr, "%s:%d connection failed.\n", host, port );
	    (void)close( fd );
	    exit( 1 );
	}

	if ( store_file( fd, sn, NULL, tname ) != 0 ) {
	    fprintf( stderr, "failed to store transcript \"%s\"\n", tname );
	    exitcode = 1;
	    (void)close( fd );
	    goto done;
	}

	if ( tran_only ) {	/* don't upload files */
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

	if ( tac == 1 ) {
	    fprintf( stderr, "Appliable transcripts cannot be uploaded.\n" );
	    exitcode = 1;
	    break;
	}

	if ( tac >= 2 && *targv[ 0 ] == 'f' ) {
	    dpath = decode( targv[ 1 ] );
	    if ( !network ) {
		if ( access( dpath,  R_OK ) < 0 ) {
		    perror( dpath );
		    exitcode = 1;
		}
	    } else if ( negative ) {
		if (( rc = n_store_file( sn, targv[ 1 ], tname )) != 0 ) {
		    fprintf( stderr, "failed to store file %s\n", dpath );
		    exitcode = 1;
		    break;
		}
	    } else {
		if (( fdt = open( dpath, O_RDONLY, 0 )) < 0 ) {
		    perror( dpath );
		    exitcode = 1;
		    break;
		} 

		rc = store_file( fdt, sn, targv[ 1 ], tname ); 
		(void)close( fdt ); 
		if ( rc != 0 ) {
		    fprintf( stderr, "failed to store file %s\n", dpath );
		    exitcode = 1;
		    break;
		}

	    }
	}
    }
    (void)fclose( fdiff );

done:
     if ( network ) {
	 if (( closesn( sn )) != 0 ) {
	     fprintf( stderr, "cannot close sn\n" );
	     exitcode = 1;
	 }
     }

    exit( exitcode );
}
