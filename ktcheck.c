#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>

#include "snet.h"
#include "connect.h"
#include "download.h"
#include "argcargv.h"
#include "chksum.h"
#include "list.h"

#define MIN(X, Y) ((X) < (Y) ? (X) : (Y))

void output( char* string);
int check( SNET *sn, char *type, char *path); 
int createspecial( SNET *sn, struct node *head );
char * getstat( SNET *sn, char *description );

void			(*logger)( char * ) = NULL;
extern struct timeval	timeout;
int			linenum = 0;
int			chksum = 1;
int			verbose = 0;
int			update = 1;
char			*command = _RADMIND_COMMANDFILE;
char			*commandpath = _RADMIND_COMMANDPATH;
char			fullpath[ MAXPATHLEN ];

extern char		*version, *checksumlist;

    char * 
getstat( SNET *sn, char *description ) 
{
    struct timeval      tv;
    char 		*line;

    if( snet_writef( sn, "STAT %s\n", description ) == NULL ) {
	perror( "snet_writef" );
	return( NULL );
    }

    if ( verbose ) printf( ">>> STAT %s\n", description );

    tv = timeout;
    if (( line = snet_getline_multi( sn, logger, &tv )) == NULL ) {
	perror( "snet_getline_multi" );
	return( NULL );
    }
    if ( *line != '2' ) {
	fprintf( stderr, "%s\n",  line );
	return( NULL );
    }

    tv = timeout;
    if (( line = snet_getline( sn, &tv )) == NULL ) {
	perror( "snet_getline 1" );
	return( NULL );
    }

    if ( verbose ) printf( "<<< %s\n", line );

    return ( line );
}

    void
output( char *string )
{
    printf( "<<< %s\n", string );
    return;
}

    int
createspecial( SNET *sn, struct node *head )
{
    int			fd;
    FILE		*fs;
    struct node 	*prev;
    char		pathdesc[ MAXPATHLEN * 2 ];
    char		*stats;

    /* Open file */
    memset( fullpath, 0, MAXPATHLEN );

    if ( snprintf( fullpath, MAXPATHLEN, "%s/special.T.%i", commandpath,
	    getpid()) > MAXPATHLEN - 1 ) {
	fprintf( stderr, "path too long: %s/special.T.%i\n", commandpath,
		(int)getpid());
	exit( 2 );
    }

    if (( fd = open( fullpath, O_WRONLY | O_CREAT | O_TRUNC, 0666 ))
	    < 0 ) {
	perror( fullpath );
	return( 1 );
    }
    if (( fs = fdopen( fd, "w" )) == NULL ) {
	perror( fullpath );
	return( 1 );
    }

    do {
	sprintf( pathdesc, "SPECIAL %s", head->path);

	if (( stats = getstat( sn, (char *)&pathdesc)) == NULL ) {
	    return( 1 );
	}

	if ( fputs( stats, fs) == EOF ) {
	    fprintf( stderr, "fputs" );
	    return( 1 );
	}
	if ( fputs( "\n", fs) == EOF ) {
	    fprintf( stderr, "fputs" );
	    return( 1 );
	}

	prev = head;
	head = head->next;

	free( prev->path );
	free( prev );
    } while ( head != NULL );

    if ( fclose( fs ) != 0 ) {
	fprintf( stderr, "flcose" );
	return( 1 );
    }

    return( 0 );
}

/*
 * return codes:
 *	0	okay
 *	1	update made
 *	2	system error
 */

    int
check( SNET *sn, char *type, char *path)
{
    char	*schksum, *stats;
    char	**targv;
    char 	pathdesc[ 2 * MAXPATHLEN ];
    char 	temppath[ 2 * MAXPATHLEN ];
    char        cchksum[ 29 ];
    int		tac;
    struct stat	st;

    if ( path != NULL ) {
	sprintf( pathdesc, "%s %s", type, path);
    } else {
	sprintf( pathdesc, "%s", type );
	path = command;
    }

    /* create full path */
    if (( strlen( command ) + strlen( commandpath ) + 2 ) >
	    MAXPATHLEN ) {
	fprintf( stderr, "path too long:%s\%s\n", commandpath,
		path );
	exit( 2 );
    }
    sprintf( fullpath, "%s/%s", commandpath, path );

    if (( stats = getstat( sn, (char *)&pathdesc)) == NULL ) {
	return( 2 );
    }

    tac = acav_parse( NULL, stats, &targv );

    if ( tac != 8 ) {
	perror( "Incorrect number of arguments\n" );
	return( 2 );
    }

    if (( schksum = strdup( targv[ 7 ] )) == NULL ) {
	perror( "strdup" );
	return( 2 );
    }

    if ( do_chksum( fullpath, cchksum ) != 0 ) {
	if ( errno != ENOENT ) {
	    fprintf( stderr, "do_chksum" );
	    return( 2 );
	}
	if ( update ) {
	    if ( retr( sn, pathdesc, fullpath, NULL, schksum,
		    (char *)&temppath ) != 0 ) {
		fprintf( stderr, "retr failed\n" );
		return( 2 );
	    }
	    if ( rename( temppath, fullpath ) != 0 ) {
		perror( temppath );
		return( 2 );
	    }
	}
	return( 1 );
    }

    if ((stat( fullpath, &st )) != 0 ) {
	perror( fullpath );
	return( 2 );
    }

    if (( strcmp( schksum, cchksum ) != 0 )
	    || ( atoi( targv[ 6 ] ) != (int)st.st_size )) {
	if ( update ) {
	    if ( unlink( fullpath ) != 0 ) {
		perror( fullpath );
		return( 2 );
	    }
	    if ( retr( sn, pathdesc, fullpath, NULL, schksum,
		    (char *)&temppath ) != 0 ) {
		fprintf( stderr, "retr failed\n" );
		return( 2 );
	    }
	    if ( rename( temppath, fullpath ) != 0 ) {
		perror( temppath );
		return( 2 );
	    }
	}
	return( 1 );
    } else {
	return( 0 );
    }

}

/*
 * exit codes:
 *      0       No changes found, everything okay
 *      1       Changes necessary / changes made
 *      2       System error
 */

    int
main( int argc, char **argv )
{
    int			c, port = htons( 6662 ), err = 0;
    int			len, tac, change = 0;
    extern int          optind;
    char		*host = _RADMIND_HOST;
    char                **targv;
    char                cline[ 2 * MAXPATHLEN ];
    char		path[ MAXPATHLEN ], temppath[ MAXPATHLEN ];
    char		lchksum[ 29 ], tchksum[ 29 ];
    struct servent	*se;
    SNET		*sn;
    FILE		*f;
    struct node		*head = NULL;
    struct stat		tst, lst;

    while (( c = getopt ( argc, argv, "c:K:nh:p:Vv" )) != EOF ) {
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
	case 'p':
	    if (( port = htons ( atoi( optarg )) ) == 0 ) {
		if (( se = getservbyname( optarg, "tcp" )) == NULL ) {
		    fprintf( stderr, "%s: service unkown\n", optarg );
		    exit( 2 );
		}
		port = se->s_port;
	    }
	    break;
	case 'K':
	    if (( command = strrchr( optarg, '/' )) == NULL ) {
		command = optarg;
		commandpath = "./";
	    } else {
		commandpath = optarg;
		*command = (char) '\0';
		command++;
	    }
	    if (( strlen( command ) + strlen( commandpath ) + 2 ) >
		    MAXPATHLEN ) {
		fprintf( stderr, "path too long:%s\%s\n", commandpath,
			command );
		exit( 2 );
	    }
	    sprintf( fullpath, "%s/%s", commandpath, command );
	    break;
	case 'n':
	    update = 0;
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

    if ( err || ( argc - optind != 0 )) {
	fprintf( stderr, "usage: ktcheck [ -nvV ] " );
	fprintf( stderr, "[ -c checksum ] [ -K command file ] " );
	fprintf( stderr, "[ -h host ] [ -p port ]\n" );
	exit( 2 );
    }

    if(( sn = connectsn( host, port )  ) == NULL ) {
	fprintf( stderr, "%s:%d connection failed.\n", host, port );
	exit( 2 );
    }

    switch( check( sn, "COMMAND", NULL )) { 
    case 0:
	break;
    case 1:
	change++;
	if ( !update ) {
	    goto done;
	}
	break;
    case 2:
	exit( 2 );
    }
    if (( strlen( command ) + strlen( commandpath ) + 2 ) >
	    MAXPATHLEN ) {
	fprintf( stderr, "path too long:%s\%s\n", commandpath,
		command );
	exit( 2 );
    }
    sprintf( fullpath, "%s/%s", commandpath, command );

    if (( f = fopen( fullpath, "r" )) == NULL ) {
	perror( fullpath );
	exit( 2 );
    }

    while ( fgets( cline, MAXPATHLEN, f ) != NULL ) {
	linenum++;

	len = strlen( cline );
	if (( cline[ len - 1 ] ) != '\n' ) {
	    fprintf( stderr, "%s: %d: line too long\n", command, linenum );
	    exit( 2 );
	}

	tac = acav_parse( NULL, cline, &targv );

	if (( tac == 0 ) || ( *targv[ 0 ] == '#' )) {
	    continue;
	}

	if ( tac != 2 ) {
	    fprintf( stderr, "%s: %d: invalid command line\n",
		    command, linenum );
	    exit( 2 );
	}

	if ( *targv[ 0 ] == 's' ) {
	    insert_node( targv[ 1 ], &head);
	    continue;
	}
	    
	switch( check( sn, "TRANSCRIPT", targv[ tac - 1] )) {
	case 0:
	    break;
	case 1:
	    change++;
	    if ( !update ) {
		goto done;
	    }
	    break;
	case 2:
	    exit( 2 );
	}
    }

    if ( head != NULL ) {
	if ( createspecial( sn, head ) != 0 ) {
	    exit( 2 );
	}
	memset( path, 0, MAXPATHLEN );
	memset( temppath, 0, MAXPATHLEN );

	if ( snprintf( path, MAXPATHLEN, "%s/special.T", commandpath ) >
		MAXPATHLEN - 1 ) {
	    fprintf( stderr, "path too long: %s/special.T\n", commandpath );
	}
	if ( snprintf( temppath, MAXPATHLEN, "%s/special.T.%i", commandpath,
		getpid()) > MAXPATHLEN - 1 ) {
	    fprintf( stderr, "path too long: %s/special.T.%i\n", commandpath,
		    (int)getpid());
	}
	if ( do_chksum( temppath, tchksum ) != 0 ) {
	    fprintf( stderr, "do_chksum failed: %s", temppath );
	    exit( 2 );
	}
	if ( do_chksum( path, lchksum ) != 0 ) {
	    if ( errno != ENOENT ) {
		fprintf( stderr, "do_chksum failed: %s\n", path );
		exit( 2 );
	    }

	    /* special.T did not exist */
	    if ( update ) { 
		if ( rename( temppath, path ) != 0 ) {
		    fprintf( stderr, "rename failed: %s %s\n", temppath, path );
		    exit( 2 );
		}
	    } else {
		/* specaial.T not updated */
		if ( unlink( temppath ) !=0 ) {
		    perror( temppath );
		    exit( 2 );
		}
	    }
	    change++;
	} else {
	    /* get file sizes */
	    if ( stat( temppath, &tst ) != 0 ) {
		perror( temppath );
		exit( 2 );
	    }
	    if ( stat( path, &lst ) != 0 ) {
		perror( path );
		exit( 2 );
	    }

	    /* specal.T exists */
	    if (( tst.st_size != lst.st_size ) ||
		    ( strcmp( tchksum, lchksum) != 0 )) {
		/* special.T new from server */
		if ( update ) {
		    if ( rename( temppath, path ) != 0 ) {
			fprintf( stderr, "rename failed: %s %s\n", temppath,
				path );
			exit( 2 );
		    }
		} else {
		    /* No update */
		    if ( unlink( temppath ) !=0 ) {
			perror( temppath );
			exit( 2 );
		}
	    }

		change++;
	    } else {

		/* specaial.T not updated */
		if ( unlink( temppath ) !=0 ) {
		    perror( temppath );
		    exit( 2 );
		}
	    }
	}
    }

    if (( closesn( sn )) !=0 ) {
	fprintf( stderr, "can not close sn\n" );
	exit( 2 );
    }

done:
    if ( change ) {
	exit( 1 );
    } else {
	exit( 0 );
    }
}
