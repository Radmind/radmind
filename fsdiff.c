#include <sys/types.h>
#include <sys/param.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <stdio.h>
#include <strings.h>

#include "transcript.h"
#include "llist.h"

extern char	*version, *checksumlist;

void	fs_walk( struct llist * );

    void
fs_walk( struct llist *path  ) 
{
    DIR			*dir;
    struct dirent	*de;
    struct llist	*head = NULL;
    struct llist	*new;
    struct llist	*cur;
    int			len;
    char		temp[ MAXPATHLEN ];

    /* call the transcript code */
    if (( transcript( &path->ll_pinfo ) == 0 ) || ( skip )) {
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

	len = strlen( path->ll_pinfo.pi_name );

	/* absolute pathname. add 2 for / and NULL termination.  */
	if (( len + strlen( de->d_name ) + 2 ) > MAXPATHLEN ) {
	    fprintf( stderr, "Absolute pathname too long\n" );
	    exit( 1 );
	}

	if ( path->ll_pinfo.pi_name[ len - 1 ] == '/' ) {
	    if ( snprintf( temp, MAXPATHLEN, "%s%s", path->ll_pinfo.pi_name,
		    de->d_name ) > MAXPATHLEN ) {
                fprintf( stderr, "%s%s: path too long\n",
                    path->ll_pinfo.pi_name, de->d_name );
		exit( 1 );
	    }
	} else {
            if ( snprintf( temp, MAXPATHLEN, "%s/%s", path->ll_pinfo.pi_name,
                    de->d_name ) > MAXPATHLEN ) {
                fprintf( stderr, "%s/%s: path too long\n",
                    path->ll_pinfo.pi_name, de->d_name );
                exit( 1 );
            }
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
    char		*kfile = _RADMIND_COMMANDFILE;
    char		*kdir = "";
    char		*p;
    int 		c, len;
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
	    kfile = optarg;
	    break;

	case '1':
	    skip = 1;
	    break;	

	case 'T':		/* want to record differences from tran */
	    edit_path = FS2TRAN;
	    break;

	case 'V':		
	    printf( "%s\n", version );
	    printf( "%s\n", checksumlist );
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

    /* Check that kfile isn't an abvious directory */
    len = strlen( kfile );
    if ( kfile[ len - 1 ] == '/' ) {
        errflag++;
    }

    if ( errflag || ( argc - optind != 1 )) {
	fprintf( stderr, "usage: fsdiff [ -T | -1 ] [ -K command ] " );
	fprintf( stderr, "[ -c chksumtype ] [ -o file ] path\n" );
	exit ( 1 );
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

    /* initialize the transcripts */
    transcript_init( kdir, kfile );
    root = ll_allocate( argv[ optind ] );

    fs_walk( root );

    /* free the transcripts */
    transcript_free( );
    hardlink_free( );
	    
    /* close the output file */     
    fclose( outtran );

    exit( 0 );	
}
