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

    static int
pathcmp( const char *dir, const char *file )
{
    return strncmp(dir, file, strlen( dir ) );
}

    void
rm_dir( char* dir, FILE *fdiff )
{
    char		tline[ 2 * MAXPATHLEN ];
    int			tac;
    char		**targv;

    printf( "with rm_dir\n" );

    /* Look at next line to see if there is a file in dir */

    printf( "  Checking for files\n" );

    if ( fgets( tline, MAXPATHLEN, fdiff ) == NULL ) {
	printf( "  End of File\n" );
	return;
    }

    tac = argcargv( tline, &targv );

    /* Next line a '-' ? */

    if ( *targv[ 0 ] != '-' ) {
	printf( "End of -'s.  Removing %s\n", dir );
    
	if ( rmdir( dir ) != 0 ) {
	    perror( dir );
	    exit( 1 );
	}
	printf( "Success!\n" );
    }

    printf( "  Found a - %s\n", targv[ 1 ] );
    printf( "  is %s in %s?\n", targv[ 2 ], dir );

    if ( pathcmp( dir, targv[ 2 ] ) == 0 ) {
	printf( "    Yes\n" );
	rm_file( targv[ 2 ] );
    } else {
	printf( "    No\n" );
    }

    return;

}

    int
main( int argc, char **argv )
{
    int			tac, fd;
    int			exitcode = 0;
    int			optind = 1;
    char		**targv;
    char		tline[ 2 * MAXPATHLEN ];
    char		type;
    extern char		*optarg;
    FILE		*fdiff; 
    struct stat		st;
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
	    printf( "Doing %s\n", tline );
	    switch( *targv[ 0 ] ) {
	    case '+':
		printf( "Have to download something\n" );
		break;
	    case '-':
		printf( "Deleting %s ", targv[ 2 ] );
		if ( lstat( targv[ 2 ], &st ) != 0 ) {
		    perror( "lstat" );
		    goto done;
		}
		if ( S_ISDIR( st.st_mode ) ) {
		    rm_dir( targv[ 2 ], fdiff );
		} else {
		    printf( "using unlink\n" );
		    if ( unlink( targv[ 2 ] ) != 0 ) {
			perror( "Unlink" );
			goto done;
		    }
		}
		
		break;
	    default:

		/* UPDATE */

		/* get file stats */

		if ( lstat( targv[ 1 ], &st ) != 0 ) {
		    perror( "lstat" );
		    goto done;
		}

		type = t_convert ( S_IFMT & st.st_mode );

		printf( "Updating %s...", targv[ 1 ] );

		/* updated based on type */

		switch ( *targv[ 0 ] ) {
		case 'f':
		    if ( type != 'f' ) {
			perror( "Should be a download\n" ); 
			goto done;
		    }

		    mode = strtol( targv[ 2 ], (char **)NULL, 8 );
		    uid = atoi( targv[ 3 ] );
		    gid = atoi( targv[ 4 ] );
		    times.modtime = atoi( targv[ 5 ] );

		    /* check mode */
		    if( mode != st.st_mode ) {
			printf( "  mode -> %o\n", mode );
			if ( chmod( targv[ 1 ], mode ) != 0 ) {
			    perror( "chmod" );
			    goto done;
			}
		    }

		    /* check uid & gid */

		    if( uid != st.st_uid  || gid != st.st_gid ) {
			if ( uid != st.st_uid ) printf( "  uid -> %i\n", (int)uid );
			if ( gid != st.st_gid ) printf( "  gid -> %i\n", (int)gid );
			if ( lchown( targv[ 1 ], uid, gid ) != 0 ) {
			    perror( "lchown" );
			    goto done;
			}
		    }

		    /* check modification time */
		    if( times.modtime != st.st_mtime ) {
			printf( "%s time -> %i\n", targv[ 1 ], (int)times.modtime );
			times.actime = st.st_atime;
			if ( utime( targv[ 1 ], &times ) != 0 ) {
			    perror( "utime" );
			    goto done;
			}
		    }
		    break;

		case 'd':

		    printf( "dir\n" );

		    mode = strtol( targv[ 2 ], (char **)NULL, 8 );
		    uid = atoi( targv[ 3 ] );
		    gid = atoi( targv[ 4 ] );

		    if ( type != 'd' ) {
			printf( "Direcetory update to non-directory\n" ); 

			if ( unlink( targv[ 1 ] ) != 0 ) {
			    perror( "unlink" );
			    goto done;
			    }
			
			if ( mkdir( targv[ 1 ], mode ) != 0 ) {
			    perror( "mkdir" );
			    goto done;
			}
		    }

		    /* check mode */
		    if( mode != st.st_mode ) {
			printf( "  mode -> %o\n", mode );
			if ( chmod( targv[ 1 ], mode ) != 0 ) {
			    perror( "chmod" );
			    goto done;
			}
		    }

		    /* check uid & gid */
		    if( uid != st.st_uid  || gid != st.st_gid ) {
			if ( uid != st.st_uid ) printf( "  uid -> %i\n", (int)uid );
			if ( gid != st.st_gid ) printf( "  gid -> %i\n", (int)gid );
			if ( lchown( targv[ 1 ], uid, gid ) != 0 ) {
			    perror( "lchown" );
			    goto done;
			}
		    }
		    printf( "  DONE\n" );
		    break;

		case 'h':
		    if ( unlink( targv[ 1 ] ) != 0 ) {
			perror( "unlink" );
			goto done;
		    }

		    if ( link( targv[ 2 ], targv[ 1 ] ) != 0 ) {
			perror( "link" );
			goto done;
		    }
		    break;
		case 'l':
		    if ( unlink( targv[ 1 ] ) != 0 ) {
			perror( "unlink" );
			goto done;
		    }
		    
		    if ( symlink( targv[ 2 ], targv[ 1 ] ) != 0 ) {
			perror( "symlink" );
			goto done;
		    }
		    break;
		case 'D':
		    if ( type != 'D' ) {
			printf( "Door update to non-door\n" ); 
		    }
		    break;
		case 's':
		    if ( type != 's' ) {
			printf( "socket update to non-socket\n" ); 
		    }
		    break;
		case 'p':
		    if ( type != 'p' ) {
			printf( "Named pipe update to non-named pipe\n" ); 
		    }
		    break;
		case 'b':
		    if ( type != 'b' ) {
			printf( "Blk special update to non-blk special\n" ); 
		    }
		    break;
		case 'c':
		    if ( type != 'c' ) {
			printf( "Char special update to non-char special\n" ); 
		    }
		    break;
		default :
		    printf( "Unkown type %c to update\n", targv[ 1 ] );
		    break;
		}
		break;  // End of defualt switch
	    }
	    printf( "Getting next line\n" );
	}
    }
done:

    fclose( fdiff );

    exit( exitcode );
}
