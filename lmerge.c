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
int		case_sensitive = 1;
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
    if ( strlen( path ) >= MAXPATHLEN ) {
	fprintf( stderr, "%s: path too long\n", path );
	return( NULL );
    }
    strcpy( new_node->path, path );

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
	fprintf( stderr, "%s: line %d: path too long\n", tran->t_tran_name,
	    tran->t_linenum );
	return( 1 );
    } 
    if ( strlen( d_path ) >= MAXPATHLEN ) {
	fprintf( stderr, "%s: line %d: %s: path too long\n",
		tran->t_tran_name, tran->t_linenum, d_path );
	return( 1 );
    }
    strcpy( tran->t_filepath, d_path );

    /* Check transcript order */
    if ( strlen( tran->t_prepath ) != 0 ) {
	 
	if ( pathcasecmp( tran->t_filepath, tran->t_prepath,
		case_sensitive ) <= 0 ) {
	    fprintf( stderr, "%s: line %d: bad sort order\n",
			tran->t_tran_name, tran->t_linenum );
	    return( 1 );
	}
    }
    if ( strlen( tran->t_filepath ) >= MAXPATHLEN ) {
	fprintf( stderr, "%s: line %d: %s: path too long\n",
		tran->t_tran_name, tran->t_linenum, tran->t_filepath );
	return( 1 );
    }
    strcpy( tran->t_prepath, tran->t_filepath );


    return( 0 );
}

    static int
copy_file( const char *src_file, const char *dest_file )
{
    int			src_fd, dest_fd = -1;
    int			rr, rc = -1;
    char		buf[ 4096 ];
    struct stat		st;

    if (( src_fd = open( src_file, O_RDONLY )) < 0 ) {
    	fprintf( stderr, "open %s failed: %s\n", src_file, strerror( errno ));
	return( rc );
    }
    if ( fstat( src_fd, &st ) < 0 ) {
	fprintf( stderr, "stat of %s failed: %s\n",
		src_file, strerror( errno ));
	goto cleanup;
    }

    if (( dest_fd = open( dest_file, O_WRONLY | O_CREAT | O_EXCL,
	    st.st_mode & 07777 )) < 0 ) {
	if ( errno == ENOENT ) {
	    rc = errno;
	} else {
	    fprintf( stderr, "open %s failed: %s\n",
		    dest_file, strerror( errno ));
	}
	goto cleanup;
    }
    while (( rr = read( src_fd, buf, sizeof( buf ))) > 0 ) {
	if ( write( dest_fd, buf, rr ) != rr ) {
	    fprintf( stderr, "write to %s failed: %s\n",
		    dest_file, strerror( errno ));
	    goto cleanup;
	}
    }
    if ( rr < 0 ){
	fprintf( stderr, "read from %s failed: %s\n",
		src_file, strerror( errno ));
	goto cleanup;
    }
    if ( fchown( dest_fd, st.st_uid, st.st_gid ) != 0 ) {
	fprintf( stderr, "chown %d:%d %s failed: %s\n",
		st.st_uid, st.st_gid, dest_file, strerror( errno ));
	goto cleanup;
    }

    rc = 0;

cleanup:
    if ( src_fd >= 0 ) {
	if ( close( src_fd ) != 0 ) {
	    fprintf( stderr, "close %s failed: %s\n",
		    src_file, strerror( errno ));
	    rc = -1;
	}
    }
    if ( dest_fd >= 0 ) {
	if ( close( dest_fd ) != 0 ) {
	    fprintf( stderr, "close %s failed: %s\n",
		    dest_file, strerror( errno ));
	    rc = -1;
	}
    }

    return( rc );
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
    int			merge_trans_only = 0;
    int			copy = 0, rc;
    char		*file = NULL;
    char		npath[ 2 * MAXPATHLEN ];
    char		opath[ 2 * MAXPATHLEN ];
    char		*radmind_path = _RADMIND_PATH;
    char		cwd[ MAXPATHLEN ];
    char		file_root[ MAXPATHLEN ];
    char		tran_root[ MAXPATHLEN ];
    char		tran_name[ MAXPATHLEN ];
    char		temp[ MAXPATHLEN ];
    struct tran		**trans = NULL;
    struct node		*new_node = NULL;
    struct node		*node = NULL;
    struct node		*dirlist = NULL;
    FILE		*ofs;
    mode_t		mask;

    while ( ( c = getopt( argc, argv, "CD:fInTu:Vv" ) ) != EOF ) {
	switch( c ) {
	case 'C':		/* copy files instead of using hardlinks */
	    copy = 1;
	    break;
	case 'D':
	    radmind_path = optarg;
	    break;
	case 'f':
	    force = 1;
	    break;
	case 'I':
	    case_sensitive = 0;
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
	case 'T':
		merge_trans_only = 1;
		break;
	default:
	    err++;
	    break;
	}
    }

    tcount = argc - ( optind + 1 );	/* "+ 1" accounts for dest tran */

    if ( merge_trans_only && force ) {
	err++;
    }
    if ( merge_trans_only && copy ) {
	err++;
    }
    if ( noupload && ( tcount > 2 ) ) {
	err++;
    }
    /* make sure there's a second transcript */
    if ( force && ( argv[ optind + 1 ] == NULL )) {
	err++;
    }
    if ( force && ( tcount > 1 ) ) {
	err++;
    }
    if ( !force && ( tcount < 2 )) {
	err++;
    }

    if ( err ) {
	fprintf( stderr, "Usage: %s [-vCIVT] [ -D path ] [ -u umask ] ",
	    argv[ 0 ] );
	fprintf( stderr, "transcript... dest\n" );
	fprintf( stderr, "       %s -f [-vCIV] [ -D path ] [ -u umask ] ",
	    argv[ 0 ] );
	fprintf( stderr, "transcript1 transcript2\n" );
	fprintf( stderr, "       %s -n [-vCIVT] [ -D path ] [ -u umask ] ",
	    argv[ 0 ] );
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
    if (( trans = (struct tran**)malloc(
	    sizeof( struct tran* ) * ( tcount ))) == NULL ) {
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

	if ( get_root( radmind_path, trans[ i ]->t_path, trans[ i ]->t_file_root,
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
	if ( strlen( trans[ 1 ]->t_file_root ) >= MAXPATHLEN ) {
	    fprintf( stderr, "%s: path too long\n", trans[ 1 ]->t_file_root );
	    exit( 2 );
	}
	strcpy( file_root, trans[ 1 ]->t_file_root );
	if ( strlen( trans[ 1 ]->t_tran_root ) >= MAXPATHLEN ) {
	    fprintf( stderr, "%s: path too long\n", trans[ 1 ]->t_tran_root );
	    exit( 2 );
	}
	strcpy( tran_root, trans[ 1 ]->t_tran_root );
	if ( strlen( trans[ 1 ]->t_tran_name ) >= MAXPATHLEN ) {
	    fprintf( stderr, "%s: path too long\n", trans[ 1 ]->t_tran_name );
	    exit( 2 );
	}
	strcpy( tran_name, trans[ 1 ]->t_tran_name );
    } else {
	/* Create tran if missing */
	if (( ofd = open( argv[ argc - 1 ], O_WRONLY | O_CREAT, 0666 ) ) < 0 ) {
	    perror( argv[ argc - 1 ] );
	    exit( 2 );
	}
	if ( close( ofd ) != 0 ) {
	    perror( argv[ argc - 1 ] );
	    exit( 2 );
	}

	/* Get paths */
	if ( *argv[ argc - 1 ] == '/' ) {
	    if ( strlen( argv[ argc - 1 ] ) >= MAXPATHLEN ) {
		fprintf( stderr, "%s: path too long\n", argv[ argc - 1 ] );
		exit( 2 );
	    }
	    strcpy( cwd, argv[ argc - 1 ] );
	} else {
	    if ( snprintf( temp, MAXPATHLEN, "%s/%s", cwd, argv[ argc - 1 ] )
		    >= MAXPATHLEN ) {
		fprintf( stderr, "%s/%s: path too long\n", cwd,
		    argv[ argc - 1 ] );
		exit( 2 );
	    }
	    strcpy( cwd, temp );
	}
	if ( get_root( radmind_path, cwd, file_root, tran_root, tran_name ) != 0 ) {
	    exit( 2 );
	}

	/* Create file/tname dir */
	if ( snprintf( npath, MAXPATHLEN, "%s/%s.%d", file_root, tran_name,
		(int)getpid()) >= MAXPATHLEN ) {
	    fprintf( stderr, "%s/%s.%d: path too long\n", file_root, tran_name,
		(int)getpid());
	    exit( 2 );
	}
	/* don't bother creating file/tname if only merging trans */
	if ( !merge_trans_only ) {
	    if ( mkdir( npath, (mode_t)0777 ) != 0 ) {
		perror( npath );
		exit( 2 );
	    }
	}
    }

    /* Create temp transcript/tname file */
    if ( snprintf( opath, MAXPATHLEN, "%s/%s.%d", tran_root, tran_name,
	    (int)getpid()) >= MAXPATHLEN ) {
	fprintf( stderr, "%s/%s.%d: path too long\n", tran_root, tran_name,
	    (int)getpid());
	exit( 2 );
    }
    if (( ofd = open( opath, O_WRONLY | O_CREAT | O_EXCL,
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
	    match = 0;

	    if ( force && ( candidate == ( tcount - 1 ))) {
		match = 1;
		goto outputline;
	    }

	    /* Compare candidate to other transcripts */
	    for ( j = i + 1; j < tcount; j++ ) {
		if ( trans[ j ]->t_eof ) {
		    continue;
		}
		cmpval = pathcasecmp( trans[ candidate ]->t_filepath,
		    trans[ j ]->t_filepath, case_sensitive );
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
				trans[ j ]->t_filepath ) >= MAXPATHLEN ) {
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
	    /* output non-files, or if we're only merging transcripts 
	     * and there is no file linking necessary
	     */
	    if (( *trans[ candidate ]->t_argv[ 0 ] != 'f'
		    && *trans[ candidate ]->t_argv[ 0 ] != 'a')
		    || merge_trans_only ) {
		goto outputline;
	    }

	    /*
	     * Assume that directory structure is present so the entire path
	     * is not recreated for every file.  Only if link fails is
	     * mkdirs() called.
	     */
	    if ( snprintf( opath, MAXPATHLEN, "%s/%s/%s",
		    trans[ candidate ]->t_file_root,
		    trans[ fileloc ]->t_tran_name,
		    trans[ candidate ]->t_filepath ) >= MAXPATHLEN ) {
		fprintf( stderr, "%s/%s/%s: path too long\n",
		    trans[ candidate ]->t_file_root,
		    trans[ fileloc ]->t_tran_name,
		    trans[ candidate ]->t_filepath );
		exit( 2 );
	    }

	    if ( !force ) {
		if ( snprintf( npath, MAXPATHLEN, "%s/%s.%d/%s",
			file_root, tran_name, (int)getpid(),
			trans[ candidate ]->t_filepath ) >= MAXPATHLEN ) {
		    fprintf( stderr, "%s/%s.%d/%s: path too long\n",
			file_root, tran_name, (int)getpid(),
			trans[ candidate ]->t_filepath );
		    exit( 2 );
		}
	    } else {
		if ( snprintf( npath, MAXPATHLEN, "%s/%s/%s", file_root,
			tran_name, trans[ candidate ]->t_filepath )
			>= MAXPATHLEN ) {
		    fprintf( stderr, "%s/%s/%s: path too long\n", 
			file_root, tran_name, trans[ candidate ]->t_filepath );
		    exit( 2 );
		}
	    }

	    /*
	     * copy or link file into new loadset. it's possible the file's
	     * directory hierarchy won't exist yet. in that case, we catch
	     * ENOENT, call mkdirs to create the parents dirs for the file,
	     * and try again. the second error is fatal.
	     */
	    if ( copy ) {
		rc = copy_file( opath, npath );
	    } else if (( rc = link( opath, npath )) != 0 ) {
		rc = errno;
	    }

	    if ( rc == ENOENT ) {

		/* If that fails, verify directory structure */
		if ( ( file = strrchr( trans[ candidate ]->t_argv[ 1 ], '/' ) )
			!= NULL ) {
		    if ( !force ) {
			if ( snprintf( npath, MAXPATHLEN,
				"%s/%s.%d/%s",
				file_root, tran_name, (int)getpid(), 
				trans[ candidate ]->t_filepath )
				>= MAXPATHLEN ) {
			    fprintf( stderr,
				"%s/%s.%d/%s: path too long\n",
				file_root, tran_name, (int)getpid(),
				trans[ candidate ]->t_filepath );
			    exit( 2 );
			}
		    } else {
			if ( snprintf( npath, MAXPATHLEN,
				"%s/%s/%s", file_root, tran_name,
				trans[ candidate ]->t_filepath )
				>= MAXPATHLEN ) {
			    fprintf( stderr,
				"%s/%s/%s: path too long\n", file_root,
				tran_name, trans[ candidate ]->t_filepath );
			    exit( 2 );
			}
		    }
		    if ( mkdirs( npath ) != 0 ) {
			fprintf( stderr, "%s: mkdirs failed\n", npath );
			exit( 2 );
		    }
		} 

		/* Try copy / link again */
    		if ( copy ) {
    		    if (( rc = copy_file( opath, npath )) != 0 ) {
    			fprintf( stderr, "copy %s to %s failed\n",
				opath, npath );
    			exit( 2 );
    		    }
    		} else if ( link( opath, npath ) != 0 ){
    		    fprintf( stderr, "link %s -> %s: %s\n",
			    opath, npath, strerror( errno ));
    		    exit( 2 );
    		}
	    } else if ( rc ) {
		if ( copy ) {
		    fprintf( stderr, "copy %s to %s failed\n", opath, npath );
		} else {
		    fprintf( stderr, "link %s to %s failed: %s\n",
			    opath, npath, strerror( rc ));
		}
		exit( 2 );
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
	    /* Don't duplicate remove line if it's not a match, or 
	     * we got -f and we're just outputing the last
	     * transcript.
	     */
	    if (( trans[ candidate ]->t_remove )
		    && !match
		    && (!( force && ( candidate == 1 )))) {
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
		tran_name, (int)getpid()) >= MAXPATHLEN ) {
	    fprintf( stderr, "%s/%s.%d: path too long\n",
		file_root, tran_name, (int)getpid());
	    exit( 2 );
	}
	if ( snprintf( npath, MAXPATHLEN, "%s/%s", file_root, tran_name )
		>= MAXPATHLEN ) {
	    fprintf( stderr, "%s/%s: path too long\n", file_root, tran_name );
	    exit( 2 );
	}
	/* don't try and move file/tname if doing client only merge,
	 * it was never created.
	 */
	if ( !merge_trans_only ) {
	    if ( rename( opath, npath ) != 0 ) {
		perror( npath );
		exit( 2 );
	    }
	}
    }
    if ( snprintf( opath, MAXPATHLEN, "%s/%s.%d", tran_root, tran_name,
	    (int)getpid()) >= MAXPATHLEN ) {
	fprintf( stderr, "%s/%s.%d: path too long\n", tran_root, tran_name,
	    (int)getpid());
	exit( 2 );
    }
    if ( snprintf( npath, MAXPATHLEN, "%s/%s", tran_root, tran_name )
	    >= MAXPATHLEN ) {
	fprintf( stderr, "%s/%s: path too long\n", tran_root, tran_name );
	exit( 2 );
    }

    if ( rename( opath, npath ) != 0 ) {
	perror( npath );
	exit( 2 );
    }

    exit( 0 );
} 
