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
    int			tac, fd;
    int			exitcode = 0;
    int			optind = 1;
    int			rval = 0;
    char		**targv;
    char		tline[ 2 * MAXPATHLEN ];
    extern char		*optarg;
    FILE		*fdiff; 
    struct stat		fileinfo;

    if (( fd = open( argv[ optind ], O_RDONLY, 0 )) < 0 ) {
	perror( argv[ optind ] );
	exit( 1 );
    }

    if (( fdiff = fdopen( fd, "r" )) == NULL ) {
	perror( "fdopen" );
	exit( 1 );
    }

    while ( fgets( tline, MAXPATHLEN, fdiff ) != NULL ) {
	tac = argcargv( tline, &targv );
	if ( tac == 1 ) {
	    printf( "Command file: %s\n", targv[ 0 ] );
	}
	else {
	    switch( *targv[ 0 ] )
	    {
		case '+':
		    printf( "Have to download something\n" );
		    break;
		case '-':
		    printf( "Deleting %s ", targv[ 2 ] );
		    switch( *targv[ 1 ] )
		    {
			case 'f':
			case 'l':
			case 'h':
			case 'b':
			case 'c':
			    printf( "using unlink\n" );
			    if ( unlink( targv[ 2 ] ) != 0 ) {
				perror( "Unlink" );
				goto done;
			    }
			    break;
			case 'd':
			    printf( "using rmdir\n" );
			    if ( rmdir( targv[ 2 ] != 0 ) ) {
				perror( "rmdir" );
				goto done;
			    }
			    break;
			default:
			    printf( "Don't know how to delete %s\n", targv[ 1 ] );
			    break;
		    }
		    break;
		case 'h':
		    printf( "Update hard link\n" );
		    break;
		case 'l':
		    printf( "Update symbolic link\n" );
		    break;
		case 'd':
		    printf( "Update directory\n" );
		    break;
		case 'c':
		    printf( "Update a character special file\n" );
		    break;
		case 'b':
		    printf( "Update a block special file\n" );
		    break;
		case 'p':
		    printf( "Update a named pipe\n" );
		    break;
		case 's':
		    printf( "Update a socket\n" );
		    break;
		case 'D':
		    printf( "Update a door\n" );
		    break;
		case 'f':
		    printf( "Update a file\n" );
		    break;
		default:
		    printf( "Don't know what to do with line\n" );
		    goto done;
	    }
	
	}

    }

done:

    fclose( fdiff );

    exit( exitcode );
}
