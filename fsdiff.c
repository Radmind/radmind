#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/mkdev.h>
#include <sys/stat.h>
#include <sys/param.h>

#include "transcript.h"
#include "llist.h"
#include "code.h"

FILE	            *out;

    void
fs_walk( struct llist *path, char *rpath ) 
{
    DIR			*dir;
    struct dirent 	*de;
    struct llist	*head = NULL;
    struct llist	*new;
    struct llist	*cur;
    char 		temp[ MAXPATHLEN ];
    int			type;

    /* if needed, make the correct path for lstat and opendir */
    if ( strcmp( rpath, path->ll_info.i_name ) != 0 ) {
        sprintf( temp, "%s/%s", rpath, path->ll_info.i_name );
    } else {
	sprintf( temp, "%s", path->ll_info.i_name );
    }

    /* call the transcript code */
    if ( transcript( path, temp, rpath, out ) == 0 ) {
	return;
    }

    /* open directory */
    if (( dir = opendir( temp )) == NULL ) {
	perror( temp );
	return;
    }

    /* read contents of directory */
    while (( de = readdir( dir )) != NULL ) {

	/* don't include . and .. */
	if (( strcmp( de->d_name, "." ) == 0 ) || 
		( strcmp( de->d_name, ".." ) == 0 )) {
	    continue;
	}

	/* make relative pathname to put in list */
	/* different from above strcpy because it is for each element
	   in the directory */
	if ( strcmp( path->ll_info.i_name, rpath ) == 0 ) {
	    sprintf( temp, "%s", de->d_name );
	} else {
	    sprintf( temp, "%s/%s", path->ll_info.i_name, de->d_name );
	}

        /* allocate new node for newly created relative pathname */
        new = ll_allocate( temp );

   	/* insert new file into the list */
	ll_insert( &head, new ); 

    }

    /* close the directory. */
    if ( closedir( dir ) != 0 ) {
    	perror( "closedir" );
	return;
    }

    /* call fswalk on each element in the sorted list */
    for ( cur = head; cur != NULL; cur = cur->ll_next ) {
	 fs_walk ( cur, rpath );
    }

    /* free list */
    ll_free( head );

    return;
}

    int
main( int argc, char **argv ) 
{
    int    		c;
    extern char    	*optarg;
    extern int    	optind;
    int    		optflag = 0;
    int    		errflag = 0;
    struct llist	*root;

    while (( c = getopt( argc, argv, "t" )) != EOF ) {
	switch( c ) {
	case 't': 		/* want to record differences from tran */
		  break;
	case '?':
		errflag++;
		break;
	default: 
		break;
	}
    }

    if ( errflag ) {
	fprintf( stderr, "usage: fsdiff [ -t ]\n" );
    }

    /* open the output file */
    if (( out = fopen( "tran.txt", "w" )) == NULL ) {
	perror( "tran.txt" );
	exit( 1 );
    }

    /* initialize the transcripts */
    transcript_init();

    /* create a linked list containing one element */
    root = ll_allocate( argv[ optind ] );

    /* go through file system */
    fs_walk( root, argv[ optind ] );

    /* free the transcripts */
    transcript_free( );
	    
    /* close the output file */     
    fclose( out );

    exit(0);	
}
