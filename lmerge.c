/*
 * Copyright (c) 2002 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>

#include "argcargv.h"
#include "code.h"
#include "list.h"
#include "mkdirs.h"
#include "pathcmp.h"

int		cksum = 1;
int		verbose = 0;
int		noupload = 0;
extern char   	*version;

struct tran {
    int                 num;
    int                 eof;
    int                 tac;
    int                 remove;
    int                 linenum;
    char                *path;
    char                *name;
    char                *line;
    char                tline[ 2 * MAXPATHLEN ];
    char                prepath[ MAXPATHLEN ];
    char                filepath[ MAXPATHLEN ];
    char                **targv;
    FILE                *fs;
    ACAV                *acav;
    struct node         *next;
};

int getnextline( struct tran *tran ); 

    int
getnextline( struct tran *tran )
{
    int len;

getline:
    if ( fgets( tran->tline, MAXPATHLEN, tran->fs ) == NULL ) {
	tran->eof = 1;
	return( 0 );
    }
    tran->linenum++;

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
    /* Skip blank lines and comments */
    if (( tran->tac == 0 ) || ( *tran->targv[ 0 ] == '#' )) {
	goto getline;
    }

    if ( *tran->targv[ 0 ] == '-' ) {
	tran->remove = 1;
	tran->targv++;
    } else {
	tran->remove = 0;
    }

    /* Decode file path */
    if ( snprintf( tran->filepath, MAXPATHLEN, "%s", decode( tran->targv[ 1 ]))
	> MAXPATHLEN -1 ) {
	fprintf( stderr, "%s: line %d: %s: path too long\n",
		tran->name, tran->linenum, decode( tran->targv[ 1 ]));
	return( 1 );
    }

    /* Check transcript order */
    if ( tran->prepath != 0 ) {
	if ( pathcmp( tran->filepath, tran->prepath ) < 0 ) {
	    fprintf( stderr, "%s: line %d: bad sort order\n",
			tran->name, tran->linenum );
	    return( 1 );
	}
    }
    if ( snprintf( tran->prepath, MAXPATHLEN, "%s", tran->filepath )
	    > MAXPATHLEN ) { 
	fprintf( stderr, "%s: line %d: %s: path too long\n",
		tran->name, tran->linenum, tran->filepath );
	return( 1 );
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
    int			force = 0, ofd, fileloc = 0;
    char		*tname = NULL, *file = NULL;
    char		*tpath = NULL;
    char		npath[ 2 * MAXPATHLEN ];
    char		opath[ 2 * MAXPATHLEN ];
    struct tran		**trans = NULL;
    struct node		*dirlist = NULL;
    FILE		*ofs;
    mode_t		mask;

    while ( ( c = getopt( argc, argv, "fnu:Vv" ) ) != EOF ) {
	switch( c ) {
	case 'f':
	    force = 1;
	    break;
	case 'n':
	    noupload = 1;
	    break;
	case 'u':
	    errno = 0;
	    mask = (mode_t)strtol( optarg, (char **)NULL, 0 );
	    if ( errno != 0 ) {
		err++;
		break;
	    }
	    umask( mask );
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
	err++;
    }
    if ( force && ( tcount > 1 ) ) {
	err++;
    }
    if ( !force && ( tcount < 2 )) {
	err++;
    }

    if ( err ) {
	fprintf( stderr, "Usage: %s [-vV] [ -u umask ] ", argv[ 0 ] );
	fprintf( stderr, "transcript... dest\n" );
	fprintf( stderr, "       %s -f [-vV] [ -u umask ] ", argv[ 0 ] );
	fprintf( stderr, "transcript1 transcript2\n" );
	fprintf( stderr, "       %s -n [-vV] [ -u umask ] ", argv[ 0 ] );
	fprintf( stderr, "transcript1 transcript2 dest\n" );
	exit( 1 );
    }

    tpath = argv[ argc - 1 ];
    if ( force ) {
	/* Check for write access */
	if ( access( argv[ argc - 1 ], W_OK ) != 0 ) {
	    perror( argv[ argc - 1 ] );
	    exit( 1 );
	}
	tcount++;			/* add dest to tran merge list */
    }

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
	trans[ i ]->linenum = 0;
	*trans[ i ]->prepath = 0;

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
	    exit( 1 );
	}
    }

    if ( force ) {
	tname = trans[ 1 ]->name;
	tpath = trans[ 1 ]->path;
    } else {
	/* Get new transcript name from transcript path */
	if ( ( tname = strrchr( tpath, '/' ) ) == NULL ) {
	    tname = tpath;
	    tpath = ".";
	} else {
	    *tname = (char)'\0';
	    tname++;
	}
    }

    if ( !force ) {
	/* Create file/tname dir */
	if ( snprintf( npath, MAXPATHLEN, "%s/../file/%s.%d", tpath, tname,
		(int)getpid()) > MAXPATHLEN -1 ) {
	    fprintf( stderr, "%s/../file/%s.%d: path too long\n", tpath, tname,
		(int)getpid());
	    exit( 1 );
	}
	if ( mkdir( npath, (mode_t)0777 ) != 0 ) {
	    perror( npath );
	    exit( 1 );
	}
    }

    /* Create temp transcript/tname file */
    if ( snprintf( opath, MAXPATHLEN, "%s/%s.%d", tpath, tname, (int)getpid())
	    > MAXPATHLEN - 1 ) {
	fprintf( stderr, "%s/%s.%d: path too long\n", tpath, tname,
	    (int)getpid());
	exit( 1 );
    }
    if ( ( ofd = open( opath, O_WRONLY | O_CREAT | O_EXCL,
	    0666 ) ) < 0 ) {
	perror( opath );
	exit( 1 );
    }
    if ( ( ofs = fdopen( ofd, "w" ) ) == NULL ) {
	perror( opath );
	exit( 1 );
    }
    
    /* merge */
    for ( i = 0; i < tcount; i++ ) {
	while ( !(trans[ i ]->eof) ) {
	    candidate = i;
	    fileloc = i;

	    if ( force && ( candidate == ( tcount - 1 ) ) ) {
		goto outputline;
	    }

	    /* Compare candidate to other transcripts */
	    for ( j = i + 1; j < tcount; j++ ) {
		if ( trans[ j ]->eof ) {
		    continue;
		}
		cmpval = pathcmp( trans[ candidate ]->filepath,
		    trans[ j ]->filepath );
		if ( cmpval == 0 ) {
		    /* File match */

		    if ( ( noupload ) &&
			    ( *trans[ candidate ]->targv[ 0 ] == 'f' 
			    || *trans[ candidate ]->targv[ 0 ] == 'a' )) {
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
		    if ( ( force ) && ( *trans[ j ]->targv[ 0 ] == 'f' 
			    || *trans[ j ]->targv[ 0 ] == 'a' )) {
			/* Remove file from lower precedence transcript */
			if ( snprintf( opath, MAXPATHLEN, "%s/../file/%s/%s",
				trans[ j ]->path, trans[ j ]->name,
				trans[ j ]->filepath ) > MAXPATHLEN -1 ) {
			    fprintf( stderr,
				"%s/../file/%s/%s: path too long\n",
				trans[ j ]->path, trans[ j ]->name,
				trans[ j ]->filepath );
			    exit( 1 );
			}
			if ( unlink( opath ) != 0 ) {
			    perror( opath );
			    exit( 1 );
			}
			if ( verbose ) printf( "%s: %s: unlinked\n",
			    trans[ j ]->name, trans[ j ]->filepath);
		    }
		    /* Advance lower precedence transcript */
		    if ( getnextline( trans[ j ] ) < 0 ) {
			exit( 1 );
		    }
		} else if ( cmpval > 0 ) {
		    candidate = j;
		    fileloc = j;
		}
	    }
	    if ( force && ( candidate == 1 ) ) {
		goto outputline;
	    }
	    /* skip items to be removed or files not uploaded */
	    if ( ( trans[ candidate ]->remove ) ||
		    ( ( noupload ) && ( candidate == 0 ) &&
			( fileloc == 0 ) ) ) {
		if ( force && ( *trans[ candidate ]->targv[ 0 ] == 'd' ) ) {
		    insert_node( trans[ candidate ]->targv[ 1 ], &dirlist );
		}
		goto skipline;
	    }
	    /* output non-files */
	    if ( *trans[ candidate ]->targv[ 0 ] != 'f'
		    && *trans[ candidate ]->targv[ 0 ] != 'a' ) {
		goto outputline;
	    }

	    /*
	     * Assume that directory structure is present so the entire path
	     * is not recreated for every file.  Only if link fails is
	     * mkdirs() called.
	     */
	    if ( snprintf( opath, MAXPATHLEN, "%s/../file/%s/%s",
		    trans[ candidate ]->path, trans[ fileloc ]->name,
		    trans[ candidate ]->filepath ) > MAXPATHLEN - 1 ) {
		fprintf( stderr, "%s/../file/%s/%s: path too long\n",
		    trans[ candidate ]->path, trans[ fileloc ]->name,
		    trans[ candidate ]->filepath );
		exit( 1 );
	    }

	    if ( !force ) {
		if ( snprintf( npath, MAXPATHLEN, "%s/../file/%s.%d/%s",
			tpath, tname, (int)getpid(),
			trans[ candidate ]->filepath ) > MAXPATHLEN - 1 ) {
		    fprintf( stderr, "%s/../file/%s.%d/%s: path too long\n",
			tpath, tname, (int)getpid(),
			trans[ candidate ]->filepath );
		    exit( 1 );
		}
	    } else {
		if ( snprintf( npath, MAXPATHLEN, "%s/../file/%s/%s", tpath,
			tname, trans[ candidate ]->filepath )
			> MAXPATHLEN - 1 ) {
		    fprintf( stderr, "%s/../file/%s/%s: path too long\n", 
			tpath, tname, trans[ candidate ]->filepath );
		    exit( 1 );
		}
	    }

	    /* First try to link file */
	    if ( link( opath, npath ) != 0 ) {

		/* If that fails, verify directory structure */
		if ( ( file = strrchr( trans[ candidate ]->targv[ 1 ], '/' ) )
			!= NULL ) {
		    if ( !force ) {
			if ( snprintf( npath, MAXPATHLEN, "%s/../file/%s.%d/%s",
				tpath, tname, (int)getpid(), 
				trans[ candidate ]->filepath )
				> MAXPATHLEN - 1 ) {
			    fprintf( stderr,
				"%s/../file/%s.%d/%s: path too long\n",
				tpath, tname, (int)getpid(),
				trans[ candidate ]->filepath );
			    exit( 1 );
			}
		    } else {
			if ( snprintf( npath, MAXPATHLEN, "%s/../file/%s/%s",
				tpath, tname, trans[ candidate ]->filepath )
				> MAXPATHLEN - 1 ) {
			    fprintf( stderr,
				"%s/../file/%s/%s: path too long\n",
				tpath, tname, trans[ candidate ]->filepath );
			    exit( 1 );
			}

		    }
		    if ( mkdirs( npath ) != 0 ) {
			fprintf( stderr, "%s: mkdirs failed\n", npath );
			exit( 1 );
		    }
		} 

		/* Try link again */
		if ( link( opath, npath ) != 0 ) {
		    fprintf( stderr, "linking %s -> %s: ",
			opath, npath );
		    perror( "" );
		    exit( 1 );
		}
	    }
	    if ( verbose ) printf( "%s: %s: merged into: %s\n",
		trans[ candidate ]->name, trans[ candidate ]->filepath,
		tname );
		
outputline:
	    /* Output line */
	    if ( fputs( trans[ candidate ]->line, ofs ) == EOF ) {
		perror( trans[ candidate ]->line );
		exit( 1 );
	    }
skipline:
	    if ( getnextline( trans[ candidate ] ) != 0 ) {
		exit( 1 );
	    }
	}
    }

    if ( force && ( dirlist != NULL ) ) {
	while ( dirlist != NULL ) {
	    if ( snprintf( opath, MAXPATHLEN, "%s/../file/%s/%s", tpath,
		    tname, dirlist->path ) > MAXPATHLEN ) {
		fprintf( stderr, "%s/../file/%s/%s: path too long\n", 
		    tpath, tname, dirlist->path );
		exit( 1 );
	    }
	    if ( unlink( opath ) != 0 ) {
		perror( opath );
		exit( 1 );
	    }
	    if ( verbose ) printf( "%s: %s: unlinked\n", tname, dirlist->path );
	    dirlist = dirlist->next;
	}
    }

    /* Rename temp transcript and file structure */
    if ( !force ) {
	if ( snprintf( opath, MAXPATHLEN, "%s/../file/%s.%d", tpath,
		tname, (int)getpid()) > MAXPATHLEN - 1 ) {
	    fprintf( stderr, "%s/../file/%s.%d: path too long\n",
		tpath, tname, (int)getpid());
	    exit( 1 );
	}
	if ( snprintf( npath, MAXPATHLEN, "%s/../file/%s", tpath, tname )
		> MAXPATHLEN - 1 ) {
	    fprintf( stderr, "%s/../file/%s: path too long\n", tpath, tname );
	    exit( 1 );
	}
	if ( rename( opath, npath ) != 0 ) {
	    perror( npath );
	    exit( 1 );
	}
    }
    if ( snprintf( opath, MAXPATHLEN, "%s/%s.%d", tpath, tname, (int)getpid())
	    > MAXPATHLEN - 1 ) {
	fprintf( stderr, "%s/%s.%d: path too long\n", tpath, tname,
	    (int)getpid());
	exit( 1 );
    }
    if ( snprintf( npath, MAXPATHLEN, "%s/%s", tpath, tname )
	    > MAXPATHLEN - 1 ) {
	fprintf( stderr, "%s/%s: path too long\n", tpath, tname );
	exit( 1 );
    }

    if ( rename( opath, npath ) != 0 ) {
	perror( npath );
	exit ( 1 );
    }

    exit( 0 );
} 
