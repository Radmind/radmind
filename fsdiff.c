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
#include <strings.h>
#include <errno.h>

#include "transcript.h"
#include "llist.h"
#include "code.h"

char	*version = VERSION;

int	main( int, char ** );
void	fs_walk( struct llist * );

    void
fs_walk( struct llist *path  ) 
{
    DIR			*dir;
    struct dirent	*de;
    struct llist	*head = NULL;
    struct llist	*new;
    struct llist	*cur;
    char		temp[ MAXPATHLEN ];

    /* call the transcript code */
    if (( transcript( &path->ll_pinfo, path->ll_pinfo.pi_name ) == 0 ) ||
	    ( skip )) {
	return;				
    }

    /* open directory */
    if (( dir = opendir( path->ll_pinfo.pi_name )) == NULL ) {
	perror( path->ll_pinfo.pi_name );
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
	if (( strlen( path->ll_pinfo.pi_name ) +
		strlen( de->d_name + 2 )) > MAXPATHLEN ) {
	    fprintf( stderr, "ERROR: Illegal length of path\n" );
	    exit( 1 );
	}

	sprintf( temp, "%s/%s", path->ll_pinfo.pi_name, de->d_name );

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
	 fs_walk ( cur );
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
    char		*cmd = "command.K";
    int 		c;
    int 		errflag = 0;

    edit_path = TRAN2FS;
    chksum = 0;
    outtran = stdout;

    while (( c = getopt( argc, argv, "c:o:K:T1V" )) != EOF ) {
	switch( c ) {
	case 'c':
	    if ( strcasecmp( optarg, "sha1" ) != 0 ) {
		perror( optarg );
		exit( 1 );
	    }
	    chksum = 1;
	    break;

	case 'o':
	    if (( outtran = fopen( optarg, "w" )) == NULL ) {
		perror( optarg );
		exit( 1 );
	    }
	    break;

	case 'K':
	    cmd = optarg;
	    break;

	case '1':
	    skip = 1;
	    break;	

	case 'T':		/* want to record differences from tran */
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

    if (( edit_path == FS2TRAN ) && ( skip )) {
	errflag++;
    }

    if ( errflag || ( argc - optind != 1 )) {
	fprintf( stderr, "usage: fsdiff [ -T | -1 ] [ -K command ] " );
	fprintf( stderr, "[ -c chksumtype ] [ -o file ] path\n" );
	exit ( 1 );
    }

    /* initialize the transcripts */
    transcript_init(  cmd );

    root = ll_allocate( argv[ optind ] );

    fs_walk( root );

    /* free the transcripts */
    transcript_free( );
    hardlink_free( );
	    
    /* close the output file */     
    fclose( outtran );

    exit( 0 );	
}
