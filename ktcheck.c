#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include <snet.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>

#include "connect.h"
#include "download.h"
#include "argcargv.h"
#include "chksum.h"
#include "list.h"

void output( char* string);
int check( SNET *sn, char *type, char *path); 
int createspecial( SNET *sn, struct node *head );
char * getstat( SNET *sn, char *description );

void			(*logger)( char * ) = NULL;
int			linenum = 0;
int			chksum = 1;
int			verbose = 0;
int			quiet = 0;
int			update = 1;
char			*kfile= _RADMIND_COMMANDFILE;
char			*kdir= "";

extern struct timeval	timeout;
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
    char		filedesc[ MAXPATHLEN * 2 ];
    char		path[ MAXPATHLEN ];
    char		*stats;

    /* Open file */
    if ( snprintf( path, MAXPATHLEN, "%sspecial.T.%i", kdir,
	    getpid()) > MAXPATHLEN - 1 ) {
	fprintf( stderr, "path too long: %sspecial.T.%i\n", kdir,
		(int)getpid());
	exit( 2 );
    }

    if (( fd = open( path, O_WRONLY | O_CREAT | O_TRUNC, 0666 ))
	    < 0 ) {
	perror( path );
	return( 1 );
    }
    if (( fs = fdopen( fd, "w" )) == NULL ) {
	perror( path );
	return( 1 );
    }

    do {
	sprintf( filedesc, "SPECIAL %s", head->path);

	if (( stats = getstat( sn, (char *)&filedesc)) == NULL ) {
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
check( SNET *sn, char *type, char *file )
{
    char	*schksum, *stats;
    char	**targv;
    char 	filedesc[ 2 * MAXPATHLEN ];
    char 	tempfile[ 2 * MAXPATHLEN ];
    char        cchksum[ 29 ];
    char	path[ MAXPATHLEN ];
    int		tac;
    struct stat	st;

    if ( file != NULL ) {
	sprintf( filedesc, "%s %s", type, file );

	/* create full path */
	if ( snprintf( path, MAXPATHLEN, "%s%s", kdir, file ) > MAXPATHLEN ) {
	    fprintf( stderr, "%s%s: path too long\n", kdir, file );
	}
    } else {
	sprintf( filedesc, "%s", type );
	file = kfile;

	/* create full path */
	if ( snprintf( path, MAXPATHLEN, "%s", kfile ) > MAXPATHLEN ) {
	    fprintf( stderr, "%s: path too long\n", kfile );
	}
    }

    if (( stats = getstat( sn, (char *)&filedesc )) == NULL ) {
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
    if ( do_chksum( path, cchksum ) != 0 ) {
	if ( errno != ENOENT ) {
	    perror( path );
	    return( 2 );
	}
	if ( update ) {
	    if ( retr( sn, filedesc, path, NULL, schksum,
		    (char *)&tempfile ) != 0 ) {
		fprintf( stderr, "%s: retr failed\n", path );
		return( 2 );
	    }
	    if ( rename( tempfile, path ) != 0 ) {
		perror( tempfile );
		return( 2 );
	    }
	    if ( !quiet ) printf( "%s: updated\n", path );
	} else {
	    if ( !quiet ) printf ( "%s: missing\n", path );
	}
	return( 1 );
    }

    if ((stat( path, &st )) != 0 ) {
	perror( path );
	return( 2 );
    }

    if (( strcmp( schksum, cchksum ) != 0 )
	    || ( atoi( targv[ 6 ] ) != (int)st.st_size )) {
	if ( update ) {
	    if ( unlink( path ) != 0 ) {
		perror( path );
		return( 2 );
	    }
	    if ( retr( sn, filedesc, path, NULL, schksum,
		    (char *)&tempfile ) != 0 ) {
		fprintf( stderr, "retr failed\n" );
		return( 2 );
	    }
	    if ( rename( tempfile, path ) != 0 ) {
		perror( path );
		return( 2 );
	    }
	    if ( !quiet ) printf( "%s: updated\n", path );
	} else {
	    if ( !quiet ) printf( "%s: out of date\n", path );
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
    char		*host = _RADMIND_HOST, *p;
    char                **targv;
    char                cline[ 2 * MAXPATHLEN ];
    char		tempfile[ MAXPATHLEN ];
    char		path[ MAXPATHLEN ];
    char		lchksum[ 29 ], tchksum[ 29 ];
    struct servent	*se;
    SNET		*sn;
    FILE		*f;
    struct node		*head = NULL;
    struct stat		tst, lst;

    while (( c = getopt ( argc, argv, "c:K:nh:p:qVv" )) != EOF ) {
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
	    kfile = optarg;
	    break;
	case 'n':
	    update = 0;
	    break;
	case 'q':
	    quiet = 1;
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

    /* Check that kfile isn't an obvious directory */
    len = strlen( kfile );
    if ( kfile[ len - 1 ] == '/' ) {
	err++;
    }

    if ( verbose && quiet ) {
	err++;
    }

    if ( err || ( argc - optind != 0 )) {
	fprintf( stderr, "usage: ktcheck [ -nvV ] " );
	fprintf( stderr, "[ -c checksum ] [ -K command file ] " );
	fprintf( stderr, "[ -h host ] [ -p port ]\n" );
	exit( 2 );
    }

    if (( kdir = strdup( kfile )) == NULL ) {
        perror( "strdup failed" );
        exit( 2 );
    }
    if (( p = strrchr( kdir, '/' )) == NULL ) {
        /* No '/' in kfile - use working directory */
        kdir = "./";
    } else {
        p++;
        *p = (char)'\0';
    }
    sprintf( path, "%s", kfile );

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

    if (( f = fopen( kfile, "r" )) == NULL ) {
	perror( kfile );
	exit( 2 );
    }

    while ( fgets( cline, MAXPATHLEN, f ) != NULL ) {
	linenum++;

	len = strlen( cline );
	if (( cline[ len - 1 ] ) != '\n' ) {
	    fprintf( stderr, "%s: %d: line too long\n", kfile, linenum );
	    exit( 2 );
	}

	tac = acav_parse( NULL, cline, &targv );

	if (( tac == 0 ) || ( *targv[ 0 ] == '#' )) {
	    continue;
	}

	if ( tac != 2 ) {
	    fprintf( stderr, "%s: %d: invalid command line\n",
		    kfile, linenum );
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

	if ( snprintf( path, MAXPATHLEN, "%sspecial.T", kdir ) >
		MAXPATHLEN - 1 ) {
	    fprintf( stderr, "path too long: %sspecial.T\n", kdir );
	}
	if ( snprintf( tempfile, MAXPATHLEN, "%sspecial.T.%i", kdir,
		getpid()) > MAXPATHLEN - 1 ) {
	    fprintf( stderr, "path too long: %sspecial.T.%i\n", kdir,
		    (int)getpid());
	}
	if ( do_chksum( tempfile, tchksum ) != 0 ) {
	    perror( tempfile );
	    exit( 2 );
	}
	if ( do_chksum( path, lchksum ) != 0 ) {
	    if ( errno != ENOENT ) {
		perror( path );
		exit( 2 );
	    }

	    /* special.T did not exist */
	    if ( update ) { 
		if ( rename( tempfile, path ) != 0 ) {
		    fprintf( stderr, "rename failed: %s %s\n", tempfile, path );
		    exit( 2 );
		}
	    } else {
		/* specaial.T not updated */
		if ( unlink( tempfile ) !=0 ) {
		    perror( tempfile );
		    exit( 2 );
		}
	    }
	    change++;
	} else {
	    /* get file sizes */
	    if ( stat( tempfile, &tst ) != 0 ) {
		perror( tempfile );
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
		    if ( rename( tempfile, path ) != 0 ) {
			fprintf( stderr, "rename failed: %s %s\n", tempfile,
				path );
			exit( 2 );
		    }
		} else {
		    /* No update */
		    if ( unlink( tempfile ) !=0 ) {
			perror( tempfile );
			exit( 2 );
		}
	    }

		change++;
	    } else {

		/* specaial.T not updated */
		if ( unlink( tempfile ) !=0 ) {
		    perror( tempfile );
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
	if ( !quiet ) printf( "No updates needed\n" );
	exit( 0 );
    }
}
