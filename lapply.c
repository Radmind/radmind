#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/mkdev.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <strings.h>
#include <utime.h>
#include <errno.h>

#include "snet.h"
#include "argcargv.h"
#include "code.h"
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
    int parentlen;

    if ( parent == NULL ) {
	printf( "  in root?" );
	return 1;
    } else {
	parentlen = strlen( parent );
	printf( "  %s a child of %s?", child, parent );
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
apply( FILE *f, char *parent )
{
    char		tline[ 2 * MAXPATHLEN ];
    int			tac, present, len;
    char		**targv, *path, *command;
    char		fstype;
    struct stat		st;
    mode_t	   	mode;
    struct utimbuf	times;
    uid_t		uid;
    gid_t		gid;

    if ( parent == NULL ) {
	printf( "\nWorking in root\n" );
    } else {
	printf( "\nWorking in %s\n", parent );
    }

    while ( fgets( tline, MAXPATHLEN, f ) != NULL ) {

	printf( "%s", tline );

	tac = argcargv( tline, &targv );

	len = strlen( tline );
        if (( tline[ len - 1 ] ) != '\n' ) {
	    fprintf( stderr, "%s: line too long\n", tline );
	    return( 1 );
	}

	if ( tac == 1 ) {
	    printf( "Command file: %s\n", targv[ 0 ] );
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

	    if ( ( fstype != *targv[ 0 ] && present ) || 
		    ( *command == '-' && present ) ) {

		if ( fstype == 'd' ) {
		    printf( "Calling apply to handle dir\n" );

		    /* Recurse */
		    apply( f, path );

		    /* Directory is now empty */
		    if ( rmdir( path ) != 0 ) {
			perror( path );
			return( 1 );	
		    }

		    present = 0;

		} else {
		    printf( "Unlinking %s for update\n", path );
		    if ( unlink( path ) != 0 ) {
			perror( path );
			return( 1 );
		    }

		    present = 0;
		}
	    }

	    /* Do transcript line */
	    if ( *command == '+' ) { 

		/* DOWNLOAD */
		printf( "Have to download something\n" );

		/* DO LSTAT ON NEW FILE */

		present = 1;
	    }

	    if ( present ) {

		/* UPDATE */
		printf( "Updating %s...", path );

		switch ( *targv[ 0 ] ) {
		case 'f':

		    if ( tac != 8 ) {
			fprintf( stderr, "%s: incorrect number of arguments\n", tline );
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
		    }

		    if( uid != st.st_uid  || gid != st.st_gid ) {
			if ( lchown( path, uid, gid ) != 0 ) {
			    perror( path );
			    return( 1 );
			}
		    }

		    if( times.modtime != st.st_mtime ) {
			
			/*Is this what I should to for access time? */
			times.actime = st.st_atime;
			if ( utime( path, &times ) != 0 ) {
			    perror( path );
			    return( 1 );
			}
		    }
		    break;

		case 'd':

		    if ( tac != 5 ) {
			fprintf( stderr, "%s: incorrect number of arguments\n", tline );
			return( 1 );
		    }

		    mode = strtol( targv[ 2 ], (char **)NULL, 8 );
		    uid = atoi( targv[ 3 ] );
		    gid = atoi( targv[ 4 ] );

		    if ( !present ) {
			if ( mkdir( path, mode ) != 0 ) {
			    perror( path );
			    return( 1 );
			}

			if ( lstat( path, &st ) !=  0 ) {
			    perror( path );
			    return( 1 );
			}
			present = 1;
		    }

		    /* check mode */
		    if( mode != st.st_mode ) {
			if ( chmod( path, mode ) != 0 ) {
			    perror( path );
			    return( 1 );
			}
		    }

		    /* check uid & gid */
		    if( uid != st.st_uid  || gid != st.st_gid ) {
			if ( lchown( path, uid, gid ) != 0 ) {
			    perror( path );
			    return( 1 );
			}
		    }
		    break;

		case 'h':

		    if ( tac != 3 ) {
			fprintf( stderr, "%s: incorrect number of arguments\n", tline );
			return( 1 );
		    }
		    if ( link( decode( targv[ 2 ] ), path ) != 0 ) {
			perror( path );
			return( 1 );
		    }
		    break;

		case 'l':

		    if ( tac != 3 ) {
			fprintf( stderr, "%s: incorrect number of arguments\n", tline );
			return( 1 );
		    }
		    if ( symlink( decode( targv[ 2 ] ), path ) != 0 ) {
			perror( path );
			return( 1 );
		    }
		    break;

		case 'p':
		    if ( tac != 5 ) { 
			fprintf( stderr, "%s: incorrect number of arguments\n", tline );
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
		    }
		    /* check uid & gid */
		    if( uid != st.st_uid  || gid != st.st_gid ) {
			if ( lchown( path, uid, gid ) != 0 ) {
			    perror( path );
			    return( 1 );
			}
		    }
		    break;

		case 'b':
		case 'c':
		    if ( tac != 7 ) {
			fprintf( stderr, "%s: incorrect number of arguments\n", tline );
			return( 1 );
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
			if ( mknod( path, mode, makedev( atoi( targv[ 6 ] ), atoi( targv[ 7 ] ) ) != 0 ) ) {
			    perror( path );
			    return( 1 );
			}
			if ( lstat( path, &st ) !=  0 ) {
			    perror( path );
			    return( 1 );
			}
			present = 1;
		    }
		    /* check mode */
		    if( mode != st.st_mode ) {
			if ( chmod( path, mode ) != 0 ) {
			    perror( path );
			    return( 1 );
			}
		    }
		    /* check uid & gid */
		    if( uid != st.st_uid  || gid != st.st_gid ) {
			if ( lchown( path, uid, gid ) != 0 ) {
			    perror( path );
			    return( 1 );
			}
		    }
		    break;
		default :
		    printf( "Unkown type %s to update\n", path );
		    return( 1 );
		} // End of defualt switch
	    }

	    /* check for child */

	    printf( "Checking child..." );

	    if ( ! ischild( parent, path ) ) {
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
