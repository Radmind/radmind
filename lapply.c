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
#include <errno.h>

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
ischild( const char *parent, const char *child)
{
    if ( parent == NULL ) {
	printf( "  in root?" );
	return 1;
    } else {
	printf( "  %s a child of %s?", child, parent );
	return !strncmp(parent, child, strlen( parent ) );
    }
}

    int 
apply( FILE *f, char *parent )
{
    char		tline[ 2 * MAXPATHLEN ];
    int			tac, present;
    char		**targv, *dir, *command;
    char		fstype;
    extern char		*optarg;
    struct stat		st;
    mode_t	   	mode;
    struct utimbuf	times;
    uid_t		uid;
    gid_t		gid;

    if ( parent != NULL ) {
	printf( "\nWorking in %s\n", parent );
    } else {
	printf( "\nWorking in root\n" );
    }

    while ( fgets( tline, MAXPATHLEN, f ) != NULL ) {

	printf( "%s", tline );

	tac = argcargv( tline, &targv );

	/* DO CHECK OF BUFFER OVERFLOW */

	if ( tac == 1 ) {
	    printf( "Command file: %s\n", targv[ 0 ] );
	} else {

	    /* Get argument offset */
	    if ( ( *targv[ 0 ] == '+' ) || ( *targv[ 0 ] == '-' ) ) {
		command = targv[ 0 ];
		targv++;
	    }

	    /* Do type check on local file */
	    if ( lstat( targv[ 1  ], &st ) ==  0 ) {
	        fstype = t_convert ( S_IFMT & st.st_mode );
		present = 1;
	    } else if ( errno != ENOENT ) { 
		perror( targv[ 1 ] );
		return( 1 );
	    } else {
		present = 0;
	    }

	    printf( " TAC = %d\n", tac );

	    if ( ( fstype != *targv[ 0 ] && present ) 
		|| 
		 ( ( tac == 9  || tac == 6 ) && *command == '-' && present ) ) {

		if ( fstype == 'd' ) {
		    printf( "Calling apply to handle dir\n" );

		    /* Recurse */
		    dir = targv[ 1 ];
		    apply( f, dir );

		    /* Directory is now empty */
		    if ( rmdir( dir ) != 0 ) {
			perror( dir );
			return( 1 );	
		    }

		    present = 0;

		} else {
		    printf( "Unlinking %s for update\n", targv[ 1 ] );
		    if ( unlink( targv[ 1 ] ) != 0 ) {
			perror( targv[ 1 ] );
			return( 1 );
		    }

		    present = 0;
		}
	    }

	    /* Do transcript line */
	    if ( *command == '+' ) { 

		/* DOWNLOAD */
		printf( "Have to download something\n" );

		present = 1;
	    }

	    if ( present ) {

		/* UPDATE */
		printf( "Updating %s...", targv[ 1 ] );

		switch ( *targv[ 0 ] ) {
		case 'f':

		    mode = strtol( targv[ 2 ], (char **)NULL, 8 );
		    uid = atoi( targv[ 3 ] );
		    gid = atoi( targv[ 4 ] );
		    times.modtime = atoi( targv[ 5 ] );

		    /* check mode */
		    if( mode != st.st_mode ) {
			printf( "  mode -> %o\n", (unsigned int)mode );
			if ( chmod( targv[ 1 ], mode ) != 0 ) {
			    perror( "targv[ 1 ]" );
			    return( 1 );
			}
		    }

		    /* check uid & gid */
		    if( uid != st.st_uid  || gid != st.st_gid ) {
			if ( uid != st.st_uid ) printf( "  uid -> %i\n", (int)uid );
			if ( gid != st.st_gid ) printf( "  gid -> %i\n", (int)gid );
			if ( lchown( targv[ 1 ], uid, gid ) != 0 ) {
			    perror( targv[ 1 ] );
			    return( 1 );
			}
		    }

		    /* check modification time */
		    if( times.modtime != st.st_mtime ) {
			printf( "%s time -> %i\n", targv[ 1 ], (int)times.modtime );
			times.actime = st.st_atime;
			if ( utime( targv[ 1 ], &times ) != 0 ) {
			    perror( targv[ 1 ] );
			    return( 1 );
			}
		    }
		    break;

		case 'd':

		    mode = strtol( targv[ 2 ], (char **)NULL, 8 );
		    uid = atoi( targv[ 3 ] );
		    gid = atoi( targv[ 4 ] );

		    if ( !present ) {
			if ( mkdir( targv[ 1 ], mode ) != 0 ) {
			    perror( targv[ 1 ] );
			    return( 1 );
			}

			if ( lchown( targv[ 1 ], uid, gid ) != 0 ) {
			    perror( targv[ 1 ] );
			    return( 1 );
			}
		    } else {

			/* check mode */
			if( mode != st.st_mode ) {
			    printf( "  mode -> %o\n", (unsigned int)mode );
			    if ( chmod( targv[ 1 ], mode ) != 0 ) {
				perror( targv[ 1 ] );
				return( 1 );
			    }
			}

			/* check uid & gid */
			if( uid != st.st_uid  || gid != st.st_gid ) {
			    if ( uid != st.st_uid ) printf( "  uid -> %i\n", (int)uid );
			    if ( gid != st.st_gid ) printf( "  gid -> %i\n", (int)gid );
			    if ( lchown( targv[ 1 ], uid, gid ) != 0 ) {
				perror( targv[ 1 ] );
				return( 1 );
			    }
			}
		    }
		    printf( "  DONE\n" );
		    break;

		case 'h':

		    /* would we ever get into this if?*/

		    if ( present ) {
			if ( unlink( targv[ 1 ] ) != 0 ) {
			    perror( targv[ 1 ] );
			    return( 1 );
			}
		    }

		    if ( link( targv[ 2 ], targv[ 1 ] ) != 0 ) {
			perror( targv[ 1 ] );
			return( 1 );
		    }
		    break;

		case 'l':

		    /* would we ever get into this if?*/

		    if ( unlink( targv[ 1 ] ) != 0 ) {
			perror( targv[ 1 ] );
			return( 1 );
		    }
		    
		    if ( symlink( targv[ 2 ], targv[ 1 ] ) != 0 ) {
			perror( targv[ 1 ] );
			return( 1 );
		    }
		    break;

		case 'p':
		    if ( fstype != 'p' ) {
			printf( "Named pipe update to non-named pipe\n" ); 
		    }
		    break;
		case 'b':
		    if ( fstype != 'b' ) {
			printf( "Blk special update to non-blk special\n" ); 
		    }
		    break;
		case 'c':
		    if ( fstype != 'c' ) {
			printf( "Char special update to non-char special\n" ); 
		    }
		    break;
		default :
		    printf( "Unkown type %s to update\n", targv[ 1 ] );
		    return( 1 );
		} // End of defualt switch
	    }

	    /* check for child */

	    printf( "Checking child..." );

	    if ( ! ischild( parent, targv[ 1 ] ) ) {
		printf( "  NO!\n" );
		return( 0 );
	    }
	    printf( "  YES!\n" );
	}

    }

    return( 0 );
}

    int
main( int argc, char **argv )
{
    FILE		*f; 

    if (( f = fopen( argv[ 1 ], "r" )) == NULL ) { 
	perror( argv[ 1 ] );
	exit( 1 );
    }

    if ( apply( f, NULL ) != 0 ) {
	fclose( f );
	exit( 1 );
    }

    exit( 0 );
}
