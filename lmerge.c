/*
 * Copyright (c) 2003 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "argcargv.h"
#include "code.h"
#include "mkdirs.h"
#include "pathcmp.h"
#include "root.h"

int		cksum = 1;
int		verbose = 0;
int		noupload = 0;
extern char   	*version;

struct node {
    char                path[ MAXPATHLEN ];
    struct node         *next;
};

struct node* create_node( char *path );
void free_node( struct node *node );

   struct node *
create_node( char *path )
{
    struct node         *new_node;

    if (( new_node = (struct node *) malloc( sizeof( struct node ))) == NULL ) {
	perror( "malloc" );
	return( NULL );
    }
    if ( snprintf( new_node->path, MAXPATHLEN, "%s", path ) >= MAXPATHLEN ) {
	fprintf( stderr, "%s: path too long\n", path );
	return( NULL );
    }

    return( new_node );
}

    void
free_node( struct node *node )
{
    free( node );
}

struct tran {
    struct node         *t_next;	/* Next tran in list */
    FILE                *t_fd;		/* open file descriptor */
    int                 t_num;		/* Tran num from command line */
    char                *t_path;	/* Path from command line */
    int                 t_eof;		/* Tran at end of file */
    int                 t_linenum;	/* Current line number */
    int                 t_remove;	/* Current line has '-' */
    char                t_prepath[ MAXPATHLEN ]; /* for order check */
    char		t_tran_root[ MAXPATHLEN ];
    char		t_file_root[ MAXPATHLEN ];
    char		t_tran_name[ MAXPATHLEN ];
    char                *t_line;
    char                t_tline[ 2 * MAXPATHLEN ];
    char                t_filepath[ MAXPATHLEN ];
    char                **t_argv;
    int                 t_tac;
    ACAV                *t_acav;
};

int getnextline( struct tran *tran ); 

    int
getnextline( struct tran *tran )
{
    int		len;
    char	*d_path;

getline:
    if ( fgets( tran->t_tline, MAXPATHLEN, tran->t_fd ) == NULL ) {
	if ( feof( tran->t_fd )) {
	    tran->t_eof = 1;
	    return( 0 );
	} else {
	    perror( tran->t_path );
	    return( -1 );
	}
    }
    tran->t_linenum++;

    if ( tran->t_line != NULL ) {
	free( tran->t_line );
	tran->t_line = NULL;
    }

    if ( ( tran->t_line = strdup( tran->t_tline ) ) == NULL ) {
	perror( tran->t_tline );
	return( -1 );
    }

    /* Check line length */
    len = strlen( tran->t_tline );
    if ( ( tran->t_tline[ len - 1 ] ) != '\n' ) {
	fprintf( stderr, "%s: %d: %s: line too long\n", tran->t_tran_name,
	    tran->t_linenum, tran->t_tline );
	return( -1 );
    }
    if ( ( tran->t_tac = acav_parse( tran->t_acav,
	    tran->t_tline, &(tran->t_argv) )  ) < 0 ) {
	fprintf( stderr, "acav_parse\n" );
	return( -1 );
    }
    /* Skip blank lines and comments */
    if (( tran->t_tac == 0 ) || ( *tran->t_argv[ 0 ] == '#' )) {
	goto getline;
    }

    if ( *tran->t_argv[ 0 ] == '-' ) {
	tran->t_remove = 1;
	tran->t_argv++;
    } else {
	tran->t_remove = 0;
    }

    /* Decode file path */
    if (( d_path = decode( tran->t_argv[ 1 ] )) == NULL ) {
	fprintf( stderr, "%s: line %d: %s: path too long\n",
		tran->t_tran_name, tran->t_linenum, decode( tran->t_argv[ 1 ]));
	return( 1 );
    } 
    if ( snprintf( tran->t_filepath, MAXPATHLEN, "%s", d_path )
	    > MAXPATHLEN - 1 ) {
	fprintf( stderr, "%s: line %d: %s: path too long\n",
		tran->t_tran_name, tran->t_linenum, d_path );
	return( 1 );
    }

    /* Check transcript order */
    if ( tran->t_prepath != 0 ) {
	if ( pathcmp( tran->t_filepath, tran->t_prepath ) < 0 ) {
	    fprintf( stderr, "%s: line %d: bad sort order\n",
			tran->t_tran_name, tran->t_linenum );
	    return( 1 );
	}
    }
    if ( snprintf( tran->t_prepath, MAXPATHLEN, "%s", tran->t_filepath )
	    >= MAXPATHLEN ) { 
	fprintf( stderr, "%s: line %d: %s: path too long\n",
		tran->t_tran_name, tran->t_linenum, tran->t_filepath );
	return( 1 );
    }

    return( 0 );
}

/*
 * exit codes:
 *	0  	okay	
 *	2	System error
 */

    int
main( int argc, char **argv )
{
    int			c, i, j, cmpval, err = 0, tcount = 0, candidate = 0;
    int			force = 0, ofd, fileloc = 0, match = 0;
    char		*file = NULL;
    char		npath[ 2 * MAXPATHLEN ];
    char		opath[ 2 * MAXPATHLEN ];
    char		cwd[ MAXPATHLEN ];
    char		file_root[ MAXPATHLEN ];
    char		tran_root[ MAXPATHLEN ];
    char		tran_name[ MAXPATHLEN ];
    struct tran		**trans = NULL;
    struct node		*new_node = NULL;
    struct node		*node = NULL;
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
	exit( 2 );
    }

    if ( force ) {
	/* Check for write access */
	if ( access( argv[ argc - 1 ], W_OK ) != 0 ) {
	    perror( argv[ argc - 1 ] );
	    exit( 2 );
	}
	tcount++;			/* add dest to tran merge list */
    }

    /* Create array of transcripts */
    if ( ( trans = (struct tran**)malloc(
	    sizeof( struct tran* ) * ( tcount ) ) ) == NULL ) {
	perror( "malloc" );
	exit( 2 );
    }
    if ( getcwd( cwd, MAXPATHLEN ) == NULL ) {
        perror( "getcwd" );
        exit( 2 );
    }

    /* loop over array of trans */
    for ( i = 0;  i < tcount;  i++ ) {

	if ( ( trans[ i ] = (struct tran*)malloc( sizeof( struct tran ) ) )
		== NULL ) {
	    perror( "malloc" );
	    return( 1 );
	}
	memset( trans[ i ], 0, sizeof( struct tran ));
	trans[ i ]->t_num = i;
	trans[ i ]->t_path = argv[ i + optind ];

	if ( get_root( trans[ i ]->t_path, trans[ i ]->t_file_root,
		trans[ i ]->t_tran_root, trans[ i ]->t_tran_name ) != 0 ) {
	    exit( 2 );
	}

	/* open tran */
	if (( trans[ i ]->t_fd = fopen( trans[ i ]->t_path, "r" )) == NULL ) {
	    perror( trans[ i ]->t_path );
	    return( 1 );
	}

	if ( ( trans[ i ]->t_acav = acav_alloc() ) == NULL ) {
	    fprintf( stderr, "acav_malloc\n" );
	    return( 1 );
	}
	trans[ i ]->t_line = NULL;
	if ( getnextline( trans[ i ] ) < 0 ) {
	    exit( 2 );
	}
    }

    if ( force ) {
	if ( snprintf( file_root, MAXPATHLEN, "%s", trans[ 1 ]->t_file_root )
		> MAXPATHLEN - 1 ) {
	    fprintf( stderr, "%s: path too long\n", trans[ 1 ]->t_file_root );
	    exit( 2 );
	}
	if ( snprintf( tran_root, MAXPATHLEN, "%s", trans[ 1 ]->t_tran_root )
		> MAXPATHLEN - 1 ) {
	    fprintf( stderr, "%s: path too long\n", trans[ 1 ]->t_tran_root );
	    exit( 2 );
	}
	if ( snprintf( tran_name, MAXPATHLEN, "%s", trans[ 1 ]->t_tran_name )
		> MAXPATHLEN - 1 ) {
	    fprintf( stderr, "%s: path too long\n", trans[ 1 ]->t_tran_name );
	    exit( 2 );
	}
    } else {
	if ( get_root( argv[ argc - 1 ], file_root, tran_root, tran_name )
		!= 0 ) {
	    exit( 2 );
	}
    }

    if ( !force ) {
	/* Create file/tname dir */
	if ( snprintf( npath, MAXPATHLEN, "%s/%s.%d", file_root, tran_name,
		(int)getpid()) > MAXPATHLEN -1 ) {
	    fprintf( stderr, "%s/%s.%d: path too long\n", file_root, tran_name,
		(int)getpid());
	    exit( 2 );
	}
	if ( mkdir( npath, (mode_t)0777 ) != 0 ) {
	    perror( npath );
	    exit( 2 );
	}
    }

    /* Create temp transcript/tname file */
    if ( snprintf( opath, MAXPATHLEN, "%s/%s.%d", tran_root, tran_name,
	    (int)getpid()) > MAXPATHLEN - 1 ) {
	fprintf( stderr, "%s/%s.%d: path too long\n", tran_root, tran_name,
	    (int)getpid());
	exit( 2 );
    }
    if ( ( ofd = open( opath, O_WRONLY | O_CREAT | O_EXCL,
	    0666 ) ) < 0 ) {
	perror( opath );
	exit( 2 );
    }
    if ( ( ofs = fdopen( ofd, "w" ) ) == NULL ) {
	perror( opath );
	exit( 2 );
    }
    
    /* merge */
    for ( i = 0; i < tcount; i++ ) {
	while ( !(trans[ i ]->t_eof)) {
	    candidate = i;
	    fileloc = i;

	    if ( force && ( candidate == ( tcount - 1 ))) {
		goto outputline;
	    }

	    /* Compare candidate to other transcripts */
	    match = 0;
	    for ( j = i + 1; j < tcount; j++ ) {
		if ( trans[ j ]->t_eof ) {
		    continue;
		}
		cmpval = pathcmp( trans[ candidate ]->t_filepath,
		    trans[ j ]->t_filepath );
		if ( cmpval == 0 ) {
		    /* File match */
		    match = 1;

		    if (( noupload ) &&
			    ( *trans[ candidate ]->t_argv[ 0 ] == 'f' 
			    || *trans[ candidate ]->t_argv[ 0 ] == 'a' )) {
			/* Use lower precedence path */
			trans[ candidate ]->t_path = 
			    trans[ j ]->t_path;

			/* Select which file should be linked */
			if ( ( strcmp( trans[ candidate ]->t_argv[ 6 ], 
				trans[ j ]->t_argv[ 6 ] ) == 0 ) &&
				( strcmp( trans[ candidate ]->t_argv[ 7 ],
				trans[ j ]->t_argv[ 7 ] ) == 0 ) ) {
			    fileloc = j;
			} else {
			    /* don't print file only in highest tran */
			    goto skipline;
			}
		    }
		    if ( ( force ) && ( *trans[ j ]->t_argv[ 0 ] == 'f' 
			    || *trans[ j ]->t_argv[ 0 ] == 'a' )) {
			/* Remove file from lower precedence transcript */
			if ( snprintf( opath, MAXPATHLEN, "%s/%s/%s",
				trans[ j ]->t_file_root,
				trans[ j ]->t_tran_name,
				trans[ j ]->t_filepath ) > MAXPATHLEN -1 ) {
			    fprintf( stderr,
				"%s/%s/%s: path too long\n",
				trans[ j ]->t_file_root,
				trans[ j ]->t_tran_name,
				trans[ j ]->t_filepath );
			    exit( 2 );
			}
			if ( unlink( opath ) != 0 ) {
			    perror( opath );
			    exit( 2 );
			}
			if ( verbose ) printf( "%s: %s: unlinked\n",
			    trans[ j ]->t_tran_name, trans[ j ]->t_filepath);
		    }
		    /* Advance lower precedence transcript */
		    if ( getnextline( trans[ j ] ) < 0 ) {
			exit( 2 );
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
	    if (( trans[ candidate ]->t_remove ) ||
		    (( noupload ) && ( candidate == 0 ) && ( fileloc == 0 ))) {
		if ( match && force &&
			( *trans[ candidate ]->t_argv[ 0 ] == 'd' )) {
		    new_node = create_node( trans[ candidate ]->t_argv[ 1 ] );
		    new_node->next = dirlist;
		    dirlist = new_node;
		}
		goto skipline;
	    }
	    /* output non-files */
	    if ( *trans[ candidate ]->t_argv[ 0 ] != 'f'
		    && *trans[ candidate ]->t_argv[ 0 ] != 'a' ) {
		goto outputline;
	    }

	    /*
	     * Assume that directory structure is present so the entire path
	     * is not recreated for every file.  Only if link fails is
	     * mkdirs() called.
	     */
	    /* Fix for broken mkdir on OS X */
	    if ( *trans[ candidate ]->t_filepath == '/' ) {
		if ( snprintf( opath, MAXPATHLEN, "%s/%s%s",
			trans[ candidate ]->t_file_root,
			trans[ fileloc ]->t_tran_name,
			trans[ candidate ]->t_filepath ) > MAXPATHLEN - 1 ) {
		    fprintf( stderr, "%s/%s/%s: path too long\n",
			trans[ candidate ]->t_file_root,
			trans[ fileloc ]->t_tran_name,
			trans[ candidate ]->t_filepath );
		    exit( 2 );
		}
	    } else {
		if ( snprintf( opath, MAXPATHLEN, "%s/%s/%s",
			trans[ candidate ]->t_file_root,
			trans[ fileloc ]->t_tran_name,
			trans[ candidate ]->t_filepath ) > MAXPATHLEN - 1 ) {
		    fprintf( stderr, "%s/%s/%s: path too long\n",
			trans[ candidate ]->t_file_root,
			trans[ fileloc ]->t_tran_name,
			trans[ candidate ]->t_filepath );
		    exit( 2 );
		}
	    }

	    if ( !force ) {
		/* Fix for broken mkdir on OS X */
		if ( *trans[ candidate ]->t_filepath == '/' ) {
		    if ( snprintf( npath, MAXPATHLEN, "%s/%s.%d/%s",
			    file_root, tran_name, (int)getpid(),
			    trans[ candidate ]->t_filepath )
			    > MAXPATHLEN - 1 ) {
			fprintf( stderr, "%s/%s.%d/%s: path too long\n",
			    file_root, tran_name, (int)getpid(),
			    trans[ candidate ]->t_filepath );
			exit( 2 );
		    }
		} else {
		    if ( snprintf( npath, MAXPATHLEN, "%s/%s.%d/%s",
			    file_root, tran_name, (int)getpid(),
			    trans[ candidate ]->t_filepath ) > MAXPATHLEN - 1 ) {
			fprintf( stderr, "%s/%s.%d/%s: path too long\n",
			    file_root, tran_name, (int)getpid(),
			    trans[ candidate ]->t_filepath );
			exit( 2 );
		    }
		}
	    } else {
		if ( snprintf( npath, MAXPATHLEN, "%s/%s/%s", file_root,
			tran_name, trans[ candidate ]->t_filepath )
			> MAXPATHLEN - 1 ) {
		    fprintf( stderr, "%s/%s/%s: path too long\n", 
			file_root, tran_name, trans[ candidate ]->t_filepath );
		    exit( 2 );
		}
	    }

	    /* First try to link file */
	    if ( link( opath, npath ) != 0 ) {

		/* If that fails, verify directory structure */
		if ( ( file = strrchr( trans[ candidate ]->t_argv[ 1 ], '/' ) )
			!= NULL ) {
		    if ( !force ) {
			/* Fix for broken mkdir on OS X */
			if ( *trans[ candidate ]->t_filepath == '/' ) {
			    if ( snprintf( npath, MAXPATHLEN,
				    "%s/%s.%d%s",
				    file_root, tran_name, (int)getpid(), 
				    trans[ candidate ]->t_filepath )
				    > MAXPATHLEN - 1 ) {
				fprintf( stderr,
				    "%s/%s.%d%s: path too long\n",
				    file_root, tran_name, (int)getpid(),
				    trans[ candidate ]->t_filepath );
				exit( 2 );
			    }
			} else {
			    if ( snprintf( npath, MAXPATHLEN,
				    "%s/%s.%d/%s",
				    file_root, tran_name, (int)getpid(), 
				    trans[ candidate ]->t_filepath )
				    > MAXPATHLEN - 1 ) {
				fprintf( stderr,
				    "%s/%s.%d/%s: path too long\n",
				    file_root, tran_name, (int)getpid(),
				    trans[ candidate ]->t_filepath );
				exit( 2 );
			    }
			}
		    } else {
			if ( *trans[ candidate ]->t_filepath == '/' ) {
			    if ( snprintf( npath, MAXPATHLEN,
				    "%s/%s%s", file_root, tran_name,
				    trans[ candidate ]->t_filepath )
				    > MAXPATHLEN - 1 ) {
				fprintf( stderr,
				    "%s/%s%s: path too long\n", file_root,
				    tran_name, trans[ candidate ]->t_filepath );
				exit( 2 );
			    }
			} else {
			    if ( snprintf( npath, MAXPATHLEN,
				    "%s/%s/%s", file_root, tran_name,
				    trans[ candidate ]->t_filepath )
				    > MAXPATHLEN - 1 ) {
				fprintf( stderr,
				    "%s/%s/%s: path too long\n", file_root,
				    tran_name, trans[ candidate ]->t_filepath );
				exit( 2 );
			    }
			}
		    }
		    if ( mkdirs( npath ) != 0 ) {
			fprintf( stderr, "%s: mkdirs failed\n", npath );
			exit( 2 );
		    }
		} 

		/* Try link again */
		if ( link( opath, npath ) != 0 ) {
		    fprintf( stderr, "linking %s -> %s: %s\n",
			opath, npath, strerror( errno ));
		    exit( 2 );
		}
	    }
	    if ( verbose ) printf( "%s: %s: merged into: %s\n",
		trans[ candidate ]->t_tran_name, trans[ candidate ]->t_filepath,
		tran_name );
		
outputline:
	    /* Output line */
	    if ( fputs( trans[ candidate ]->t_line, ofs ) == EOF ) {
		perror( trans[ candidate ]->t_line );
		exit( 2 );
	    }
skipline:
	    if (( trans[ candidate ]->t_remove ) && !match ) {
		/* Recreate unmatched "-" line */
		if ( fputs( trans[ candidate ]->t_line, ofs ) == EOF ) {
		    perror( trans[ candidate ]->t_line );
		    exit( 2 );

		}
	    }
	    if ( getnextline( trans[ candidate ] ) != 0 ) {
		exit( 2 );
	    }
	}
    }

    if ( force ) {
	while ( dirlist != NULL ) {
	    node = dirlist;
	    dirlist = node->next;
	    if ( snprintf( opath, MAXPATHLEN, "%s/%s/%s", file_root,
		    tran_name, node->path ) >= MAXPATHLEN ) {
		fprintf( stderr, "%s/%s/%s: path too long\n", 
		    file_root, tran_name, node->path );
		exit( 2 );
	    }
	    if ( rmdir( opath ) != 0 ) {
		if (( errno == EEXIST ) || ( errno == ENOTEMPTY )) {
		    fprintf( stderr, "%s: %s: Not empty, continuing...\n",
			tran_name, node->path );
		} else if ( errno != ENOENT ) {
		    perror( opath );
		    exit( 2 );
		}
	    } else {
		if ( verbose ) printf( "%s: %s: unlinked\n", tran_name,
		    node->path );
	    }
	    free_node( node );
	}
    }

    /* Rename temp transcript and file structure */
    if ( !force ) {
	if ( snprintf( opath, MAXPATHLEN, "%s/%s.%d", file_root,
		tran_name, (int)getpid()) > MAXPATHLEN - 1 ) {
	    fprintf( stderr, "%s/%s.%d: path too long\n",
		file_root, tran_name, (int)getpid());
	    exit( 2 );
	}
	if ( snprintf( npath, MAXPATHLEN, "%s/%s", file_root, tran_name )
		> MAXPATHLEN - 1 ) {
	    fprintf( stderr, "%s/%s: path too long\n", file_root, tran_name );
	    exit( 2 );
	}
	if ( rename( opath, npath ) != 0 ) {
	    perror( npath );
	    exit( 2 );
	}
    }
    if ( snprintf( opath, MAXPATHLEN, "%s/%s.%d", tran_root, tran_name,
	    (int)getpid()) > MAXPATHLEN - 1 ) {
	fprintf( stderr, "%s/%s.%d: path too long\n", tran_root, tran_name,
	    (int)getpid());
	exit( 2 );
    }
    if ( snprintf( npath, MAXPATHLEN, "%s/%s", tran_root, tran_name )
	    > MAXPATHLEN - 1 ) {
	fprintf( stderr, "%s/%s: path too long\n", tran_root, tran_name );
	exit( 2 );
    }

    if ( rename( opath, npath ) != 0 ) {
	perror( npath );
	exit( 2 );
    }

    exit( 0 );
} 
