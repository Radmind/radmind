#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/mkdev.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <unistd.h>
#include <errno.h>
 
#include "transcript.h"
#include "llist.h"
#include "code.h"

#define FS2TRAN		0
#define TRAN2FS		1

static int		skip;

void fs_walk( struct llist *, int );

    void
fs_walk( struct llist *path, int rpath ) 
{
    DIR			*dir;
    struct dirent 	*de;
    struct llist	*head = NULL;
    struct llist	*new;
    struct llist	*cur;
    char 		temp[ MAXPATHLEN ];

    /* call the transcript code */
    if (( transcript( &path->ll_info, path->ll_info.i_name, outtran ) == 0 ) 
		|| ( skip == 1 )) {
	return;
    }

    /* open directory */
    if (( dir = opendir( path->ll_info.i_name )) == NULL ) {
	perror( path->ll_info.i_name );
	return;
    }

    /* read contents of directory */
    while (( de = readdir( dir )) != NULL ) {

	/* don't include . and .. */
	if (( strcmp( de->d_name, "." ) == 0 ) || 
		( strcmp( de->d_name, ".." ) == 0 )) {
	    continue;
	}

	/* construct relative pathname to put in list */
	if ( rpath == 0 ) {
	    if (( strlen( path->ll_info.i_name ) + strlen( de->d_name + 2 )) > 
			MAXPATHLEN ) {
		fprintf( stderr, "ERROR: Illegal length of path\n" );
		exit( 1 );
	    }
	    sprintf( temp, "%s/%s", path->ll_info.i_name, de->d_name );
	} else {
	    sprintf( temp, "%s", de->d_name );
	}

        /* allocate new node for newly created relative pathname */
        new = ll_allocate( temp );

   	/* insert new file into the list */
	ll_insert( &head, new ); 

    }

    if ( closedir( dir ) != 0 ) {
    	perror( "closedir" );
	return;
    }

    /* call fswalk on each element in the sorted list */
    for ( cur = head; cur != NULL; cur = cur->ll_next ) {
	 fs_walk ( cur, 0 );
    }

    ll_free( head );

    return;
}

    int
main( int argc, char **argv ) 
{
    int    		c;
    extern char    	*optarg;
    extern int    	optind;
    extern int		errno;
    int    		errflag = 0;
    struct llist	*root;

    skip = 0;
    edit_path = TRAN2FS;
    outtran = stdout;

    while (( c = getopt( argc, argv, "o:t1" )) != EOF ) {
	switch( c ) {
	case 'o':
		  if (( outtran = fopen( optarg, "w" )) == NULL ) {
			perror( optarg );
			exit( 1 );
		  }
		  break;
	case '1':
		  skip = 1;
		  break;	
	case 't': 		/* want to record differences from tran */
		  edit_path = FS2TRAN;
		  break;
	case '?':
		errflag++;
		break;
	default: 
		break;
	}
    }

    if ( errflag ) {
	fprintf( stderr, "usage: fsdiff [ -t | -1 | -o <file> ]\n" );
    }

    if ( argv[ optind ] == NULL ) {
	fprintf( stderr, "ERROR: A valid path name is required\n" );
	exit( 1 );
    }

    /* initialize the transcripts */
    transcript_init();

    if (( chdir( argv[ optind ] ) != 0 ) || ( skip == 1 )) {
	if (( errno != ENOTDIR ) && ( skip == 0 )) {
		perror( argv[ optind ] );
		exit( 1 );
	} else {
    		root = ll_allocate( argv[ optind ] );
	}
    } else {
    	root = ll_allocate( "." );
    }
    fs_walk( root, 1 );

    /* free the transcripts */
    transcript_free( );
	    
    /* close the output file */     
    fclose( outtran );

    exit(0);	
}
