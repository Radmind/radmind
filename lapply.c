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
#include <utime.h>

#include "snet.h"
#include "argcargv.h"

    static char
t_convert( int type )
{
    switch( type ) {
    case S_IFREG:
	return ( 'f' );
    case S_IFDIR:
	return ( 'd' );
    case S_IFLNK:
	return ( 'l' );
    case S_IFCHR:
	return ( 'c' );
    case S_IFBLK:
	return ( 'b' );
#ifdef SOLARIS
    case S_IFDOOR:
	return ( 'D' );
#endif
    case S_IFIFO:
	return ( 'p' );
    case S_IFSOCK:
	return ( 's' );
    default:
	return ( 0 );
    }
}

    int
main( int argc, char **argv )
{
    int			tac, fd;
    int			exitcode = 0;
    int			optind = 1;
    char		**targv;
    char		tline[ 2 * MAXPATHLEN ];
    extern char		*optarg;
    FILE		*fdiff; 
    struct stat		fileinfo;
    mode_t	   	mode;
    struct utimbuf	times;
    uid_t		uid;
    gid_t		gid;

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
	    switch( *targv[ 0 ] ) {
	    case '+':
		printf( "Have to download something\n" );
		break;
	    case '-':
		printf( "Deleting %s ", targv[ 2 ] );
		if ( lstat( targv[ 2 ], &fileinfo ) != 0 ) {
		    perror( "lstat" );
		    goto done;
		}
		if ( S_ISDIR( fileinfo.st_mode ) ) {
		    printf( "using rmdir\n" );
		    if ( rmdir( targv[ 2 ] ) != 0 ) {
			perror( "rmdir" );
			goto done;
		    }
		} else {
		    printf( "using unlink\n" );
		    if ( unlink( targv[ 2 ] ) != 0 ) {
			perror( "Unlink" );
			goto done;
		    }
		}
		break;
	    default:

		printf( "updating %s\n", targv[ 1 ] );

		mode = strtol( targv[ 2 ], (char **)NULL, 8 );
		uid = atoi( targv[ 3 ] );
		gid = atoi( targv[ 4 ] );
		times.modtime = atoi( targv[ 5 ] );

		if ( lstat( targv[ 1 ], &fileinfo ) != 0 ) {
		    perror( "lstat" );
		    goto done;
		}

		/* check mode */

		if( mode != fileinfo.st_mode ) {
		    printf( "  mode -> %o\n", mode );
		    if ( chmod( targv[ 1 ], mode ) != 0 ) {
			perror( "chmod" );
			goto done;
		    }
		}

		/* check uid & gid */

		if( uid != fileinfo.st_uid  || gid != fileinfo.st_gid ) {
		    if ( uid != fileinfo.st_uid ) printf( "  uid -> %i\n", (int)uid );
		    if ( gid != fileinfo.st_gid ) printf( "  gid -> %i\n", (int)gid );
		    if ( lchown( targv[ 1 ], uid, gid ) != 0 ) {
			perror( "lchown" );
			goto done;
		    }
		}

		/* check modification time */

		if( times.modtime != fileinfo.st_mtime ) {
		    printf( "%s time -> %i\n", targv[ 1 ], (int)times.modtime );
		    times.actime = fileinfo.st_atime;
		    if ( utime( targv[ 1 ], &times ) != 0 ) {
			perror( "utime" );
			goto done;
		    }
		}
	    }
	}
    }
done:

    fclose( fdiff );

    exit( exitcode );
}
