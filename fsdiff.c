#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>

#ifdef SOLARIS
#include <sys/mkdev.h>
#endif

#include <sys/stat.h>
#include <sys/param.h>
#include <unistd.h>
#include <errno.h>

#include "transcript.h"
#include "llist.h"
#include "code.h"

char	*version = VERSION;

int	main( int, char ** );
void	fs_walk( struct llist *, int );

    void
fs_walk( struct llist *path, int flag ) 
{
    DIR			*dir;
    struct dirent	*de;
    struct llist	*head = NULL;
    struct llist	*new;
    struct llist	*cur;
    char		temp[ MAXPATHLEN ];

    /* call the transcript code */
    if (( transcript( &path->ll_info, path->ll_info.i_name ) == 0 ) ||
	    ( flag & FLAG_SKIP )) {
	return;				
    }

    /* open directory */
    if (( dir = opendir( path->ll_info.i_name )) == NULL ) {
	perror( path->ll_info.i_name );
	exit( 1 );	
    }

    /* read contents of directory */
    while (( de = readdir( dir )) != NULL ) {

	/* don't include . and .. */
	if (( strcmp( de->d_name, "." ) == 0 ) || 
		( strcmp( de->d_name, ".." ) == 0 )) {
	    continue;
	}

	/* construct relative pathname to put in list */
	if ( !( flag & FLAG_INIT )) {
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
	exit( 1 );
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
    struct llist	*root;
    extern char 	*optarg;
    extern int		optind;
#ifndef linux
    extern int		errno;
#endif
    int 		c;
    int 		errflag = 0;
    int			flag = 0;	/* XXX do we need a flag  */  

    flag |= FLAG_INIT;
    edit_path = TRAN2FS;
    outtran = stdout;

    while (( c = getopt( argc, argv, "o:t1V" )) != EOF ) {
	switch( c ) {
	case 'o':
	    if (( outtran = fopen( optarg, "w" )) == NULL ) {
		perror( optarg );
		exit( 1 );
	    }
	    break;

	case '1':
	    flag |= FLAG_SKIP;
	    break;	

	case 't':		/* want to record differences from tran */
	    edit_path = FS2TRAN;
	    break;

	case 'V':		
	    printf( "%s\n", version );
	    exit( 0 );

	case '?':
	    errflag++;
	    break;

	default: 
	    break;
	}
    }

    if (( edit_path == FS2TRAN ) && ( flag & FLAG_SKIP )) {
	errflag++;
    }

    if ( errflag || ( argc - optind != 1 )) {
	fprintf( stderr, "usage: fsdiff [ -t | -1 ] [ -o file ] path\n" );
	exit ( 1 );
    }

    /* initialize the transcripts */
    transcript_init( flag );

    if ( flag & FLAG_SKIP ) {
	root = ll_allocate( argv[ optind ] );
    } else if ( chdir( argv[ optind ] ) != 0 ) {
	if ( errno != ENOTDIR ) {
	    perror( argv[ optind ] );
	    exit( 1 );
	}
	root = ll_allocate( argv[ optind ] );
    } else {
	root = ll_allocate( "." );
    }
    fs_walk( root, flag );

    /* free the transcripts */
    transcript_free( );
    hardlink_free( );
	    
    /* close the output file */     
    fclose( outtran );

    exit( 0 );	
}
