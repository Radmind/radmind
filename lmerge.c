#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <sha.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/mkdev.h>
#include <sys/ddi.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <strings.h>

#include "argcargv.h"
#include "lmerge.h"
#include "pathcmp.h"
#include "mkdirs.h"

int		chksum = 1;
int		verbose = 0;
int		noupload = 0;
extern char   	*version;

    int
getnextline( struct tran *tran )
{
    int len;

    if ( fgets( tran->tline, MAXPATHLEN, tran->fs ) == NULL ) {
	tran->eof = 1;
	return( 0 );
    }

    if ( tran->line != NULL ) {
	free( tran->line );
	tran->line = NULL;
    }

    if ( ( tran->line = strdup( tran->tline ) ) == NULL ) {
	perror( tran->tline );
	return( -1 );
    }

    /* Check line length */
    len = strlen( tran->tline );
    if ( ( tran->tline[ len - 1 ] ) != '\n' ) {
	fprintf( stderr, "%s: line too long\n", tran->tline );
	return( -1 );
    }
    if ( ( tran->tac = acav_parse( tran->acav,
	    tran->tline, &(tran->targv) )  ) < 0 ) {
	fprintf( stderr, "acav_parse\n" );
	return( -1 );
    }

    if ( *tran->targv[ 0 ] == '-' ) {
	tran->remove = 1;
	tran->targv++;
    } else {
	tran->remove = 0;
    }
    return( 0 );
}

/*
 * exit codes:
 *	0  	okay	
 *	1	System error
 */

    int
main( int argc, char **argv )
{
    int			c, i, j, cmpval, err = 0, tcount = 0, candidate = NULL;
    int			ofd, fileloc = 0;
    char		*tname = NULL, *file = NULL;
    char		*tpath = NULL;
    char		npath[ 2 * MAXPATHLEN ];
    char		opath[ 2 * MAXPATHLEN ];
    struct tran		**trans = NULL;
    FILE		*ofs;

    while ( ( c = getopt( argc, argv, "nVv" ) ) != EOF ) {
	switch( c ) {
	case 'n':
	    noupload = 1;
	    break;
	case 'V':
	    printf( "%s\n", version );
	    exit( 0 );
	case 'v':
	    verbose = 1;
	    break;
	default:
	    err++;
	    break;
	}
    }

    tcount = argc - ( optind + 1 );	/* "+ 1" accounts for dest tran */

    if ( noupload && ( tcount > 2 ) ) {
	fprintf( stderr, "usage: lmerge [ -nvV ] " );
	fprintf( stderr, "transcript1, transcript2, dest\n" );
	exit( 2 );
    }

    if ( err || ( ( argc - optind ) < 2 ) ) {
	fprintf( stderr, "usage: lmerge [ -vV ] " );
	fprintf( stderr, "transcript1, transcript2, ..., dest\n" );
	exit( 2 );
    }

    tpath = argv[ argc - 1 ];

    /* Create array of transcripts */
    if ( ( trans = (struct tran**)malloc(
	    sizeof( struct tran* ) * ( tcount ) ) ) == NULL ) {
	perror( "malloc" );
	exit( 1 );
    }

    /* loop over array of trans */
    for ( i = 0;  i < tcount;  i++ ) {

	if ( ( trans[ i ] = (struct tran*)malloc( sizeof( struct tran ) ) )
		== NULL ) {
	    perror( "malloc" );
	    return( 1 );
	}
	trans[ i ]->num = i;
	trans[ i ]->eof = 0;

	/* open tran */
	if ( ( trans[ i ]->fs = fopen( argv[ i + optind ], "r" ) ) == NULL ) {
	    perror( argv[ i + optind ] );
	    return( 1 );
	}

	/* Get transcript name from path */
	trans[ i ]->path = argv[ i + optind ];
	if ( ( trans[ i ]->name = strrchr( trans[ i ]->path, '/' ) ) == NULL ) {
	    trans[ i ]->name = trans[ i ]->path;
	    trans[ i ]->path = ".";
	} else {
	    *trans[ i ]->name = (char)'\0';
	    trans[ i ]->name++;
	}

	if ( ( trans[ i ]->acav = acav_alloc() ) == NULL ) {
	    fprintf( stderr, "acav_malloc\n" );
	    return( 1 );
	}
	trans[ i ]->line = NULL;
	if ( getnextline( trans[ i ] ) < 0 ) {
	    fprintf( stderr, "getnextline\n" );
	    exit( 1 );
	}
    }

    /* Get new transcript name from transcript path */
    if ( ( tname = strrchr( tpath, '/' ) ) == NULL ) {
	tname = tpath;
	tpath = ".";
    } else {
	*tname = (char)'\0';
	tname++;
    }

    /* Create file/tname dir */
    sprintf( npath, "%s/../file/%s.%d", tpath, tname, (int)getpid() );
    if ( mkdir( npath, 0777 ) != 0 ) {
	perror( npath );
	exit( 1 );
    }

    /* Create transcript/tname file */
    sprintf( opath, "%s/%s.%d", tpath, tname, (int)getpid() );
    if ( ( ofd = open( opath, O_WRONLY | O_CREAT | O_EXCL,
	    0666 ) ) < 0 ) {
	perror( opath );
	exit( 1 );
    }
    if ( ( ofs = fdopen( ofd, "w" ) ) == NULL ) {
	perror( opath );
	exit( 1 );
    }

    /* Merge transcripts */
    for ( i = 0; i < tcount; i++ ) {
	while ( !(trans[ i ]->eof) ) {
	    candidate = i;
	    fileloc = i;

	    /* Compare candidate to other transcripts */
	    for ( j = i + 1; j < tcount; j++ ) {
		if ( trans[ j ]->eof ) {
		    continue;
		}
		cmpval = pathcmp( trans[ candidate ]->targv[ 1 ],
		    trans[ j ]->targv[ 1 ] );
		if ( cmpval == 0 ) {

		    if ( ( noupload ) && ( *trans[ candidate ]->targv[ 0 ]
			    == 'f' ) ) {
			/* Use lower precedence path */
			trans[ candidate ]->path = 
			    trans[ j ]->path;

			/* Select which file should be linked */
			if ( ( strcmp( trans[ candidate ]->targv[ 6 ], 
				trans[ j ]->targv[ 6 ] ) == 0 ) &&
				( strcmp( trans[ candidate ]->targv[ 7 ],
				trans[ j ]->targv[ 7 ] ) == 0 ) ) {
			    fileloc = j;
			} else {
			    /* don't print file only in highest tran */
			    goto skipline;
			}
		    }
		    /* Advance lower precedence transcript */
		    if ( getnextline( trans[ j ] ) < 0 ) {
			fprintf( stderr, "getnextline\n" );
			exit( 1 );
		    }
		} else if ( cmpval > 0 ) {
		    candidate = j;
		    fileloc = j;
		}
	    }
	    /* output non-files */
	    if ( *trans[ candidate ]->targv[ 0 ] != 'f' ) {
		goto outputline;
	    }
	    /* skip items to be removed or files not uploaded */
	    if ( ( trans[ candidate ]->remove ) ||
		    ( ( noupload ) && ( candidate == 0 ) &&
		    ( fileloc == 0 ) ) ) {
		goto skipline;
	    }

	    /* Link file */
	    sprintf( npath, "%s/../file/%s.%d/%s", tpath, tname,
		(int)getpid(), trans[ candidate ]->targv[ 1 ] );
	    sprintf( opath,"%s/../file/%s/%s", trans[ candidate ]->path,
		trans[ fileloc ]->name, trans[ candidate ]->targv[ 1 ] );

	    /*
	     * Assume that directory structure is present so the entire path
	     * is not recreated for every file.  Only if link fails is
	     * create_direcetories() called.
	     */

	    /* First try to link file */
	    if ( link( opath, npath ) != 0 ) {

		/* If that fails, verify directory structure */
		if ( ( file = strrchr( trans[ candidate ]->targv[ 1 ], '/' ) )
			!= NULL ) {
		    sprintf( npath, "%s/../file/%s.%d/%s", tpath,
			tname, (int)getpid(), trans[ candidate ]->targv[ 1 ] ); 
		    if ( create_directories( npath ) != 0 ) {
			fprintf( stderr, "create_dirs\n" );
			exit( 1 );
		    }
		} 

		/* Try link again */
		if ( link( opath, npath ) != 0 ) {
		    fprintf( stderr, "creating %s by linking to %s",
			npath, opath );
		    perror( "" );
		    exit( 1 );
		}
	    }
	    if ( verbose ) printf( "*** %d: linked %s/%s\n",
		trans[ fileloc ]->num, tname, trans[ candidate ]->targv[ 1 ]);
		
outputline:
	    if ( fputs( trans[ candidate ]->line, ofs ) == EOF ) {
		perror( trans[ candidate ]->line );
		exit( 1 );
	    }
skipline:
	    if ( getnextline( trans[ candidate ] ) != 0 ) {
		fprintf( stderr, "getnextline\n" );
		exit( 1 );
	    }
	}
    }

    /* Rename temp transcript and file structure */
    sprintf( opath, "%s/../file/%s.%d", tpath, tname, (int)getpid() );
    sprintf( npath, "%s/../file/%s", tpath, tname );
    if ( rename( opath, npath ) != 0 ) {
	perror( npath );
	exit( 1 );
    }
    sprintf( opath, "%s/%s.%d", tpath, tname, (int)getpid() );
    sprintf( npath, "%s/%s", tpath, tname );
    if ( rename( opath, npath ) != 0 ) {
	perror( npath );
	exit ( 1 );
    }

    exit( 0 );
} 
