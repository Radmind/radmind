/*
 * Copyright (c) 2003 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/param.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>

#include <openssl/evp.h>

#include "applefile.h"
#include "transcript.h"
#include "llist.h"
#include "pathcmp.h"

void            (*logger)( char * ) = NULL;

extern char	*version, *checksumlist;

void		fs_walk( char *, int, int );
int		dodots = 0;
int		lastpercent = -1;
const EVP_MD    *md;

    void
fs_walk( char *path, int start, int finish ) 
{
    DIR			*dir;
    struct dirent	*de;
    struct llist	*head = NULL;
    struct llist	*new;
    struct llist	*cur;
    int			len;
    int			count = 0;
    float		chunk, f = start;
    char		temp[ MAXPATHLEN ];
    struct transcript	*tran;

    if (( finish > 0 ) && ( start != lastpercent )) {
	lastpercent = start;
	printf( "%%%.2d %s\n", start, path );
	fflush( stdout );
    }

    /* call the transcript code */
    switch ( transcript( path )) {
    case 2 :			/* negative directory */
	for (;;) {
	    tran = transcript_select();
	    if ( tran->t_eof ) {
		return;
	    }

	    if ( ischild( tran->t_pinfo.pi_name, path )) {
		/*
		 * XXX
		 * This strcpy() is not itself dangerous, because pi_name
		 * is a MAXPATHLEN-sized buffer.  However, it does not appear
		 * that copies into pi_name are carefully checked.
		 */
		strcpy( temp, tran->t_pinfo.pi_name );
		fs_walk( temp, start, finish );
	    } else {
		return;
	    }
	}

    case 0 :			/* not a directory */
	return;
    case 1 :			/* directory */
	if ( skip ) {
	    return;
	}
	break;
    default :
	fprintf( stderr, "transcript returned an unexpect value!\n" );
	exit( 2 );
    }

    /* open directory */
    if (( dir = opendir( path )) == NULL ) {
	perror( path );
	exit( 2 );	
    }

    /* read contents of directory */
    while (( de = readdir( dir )) != NULL ) {

	/* don't include . and .. */
	if (( strcmp( de->d_name, "." ) == 0 ) || 
		( strcmp( de->d_name, ".." ) == 0 )) {
	    continue;
	}

	count++;
	len = strlen( path );

	/* absolute pathname. add 2 for / and NULL termination.  */
	if (( len + strlen( de->d_name ) + 2 ) > MAXPATHLEN ) {
	    fprintf( stderr, "Absolute pathname too long\n" );
	    exit( 2 );
	}

	if ( path[ len - 1 ] == '/' ) {
	    if ( snprintf( temp, MAXPATHLEN, "%s%s", path, de->d_name )
		    >= MAXPATHLEN ) {
                fprintf( stderr, "%s%s: path too long\n", path, de->d_name );
		exit( 2 );
	    }
	} else {
            if ( snprintf( temp, MAXPATHLEN, "%s/%s", path, de->d_name )
		    >= MAXPATHLEN ) {
                fprintf( stderr, "%s/%s: path too long\n", path, de->d_name );
                exit( 2 );
            }
	}

	/* allocate new node for newly created relative pathname */
	new = ll_allocate( temp );

	/* insert new file into the list */
	ll_insert( &head, new ); 
    }

    chunk = (( finish - start ) / ( float )count );

    if ( closedir( dir ) != 0 ) {
	perror( "closedir" );
	exit( 2 );
    }

    /* call fswalk on each element in the sorted list */
    for ( cur = head; cur != NULL; cur = cur->ll_next ) {
	fs_walk( cur->ll_name, ( int )f, ( int )( f + chunk ));
	f += chunk;
    }

    ll_free( head );

    return;
}

    int
main( int argc, char **argv ) 
{
    extern char 	*optarg;
    extern int		optind;
    char		*kfile = _RADMIND_COMMANDFILE;
    int			gotkfile = 0;
    int 		c, len, edit_path_change = 0;
    int 		errflag = 0, use_outfile = 0;
    int			finish = 0;
    struct stat		st;

    edit_path = CREATABLE;
    cksum = 0;
    outtran = stdout;

    while (( c = getopt( argc, argv, "%Ac:Co:K:1Vv" )) != EOF ) {
	switch( c ) {
	case '%':
	case 'v':
	    finish = 100;
	    break;

	case 'c':
            OpenSSL_add_all_digests();
            md = EVP_get_digestbyname( optarg );
            if ( !md ) {
                fprintf( stderr, "%s: unsupported checksum\n", optarg );
                exit( 2 );
            }
            cksum = 1;
            break;

	case 'o':
	    if (( outtran = fopen( optarg, "w" )) == NULL ) {
		perror( optarg );
		exit( 2 );
	    }
	    use_outfile = 1;
	    break;

	case 'K':
	    kfile = optarg;
	    gotkfile = 1;
	    break;

	case '1':
	    skip = 1;
	case 'C':
	    edit_path_change++;
	    edit_path = CREATABLE;
	    break;	

	case 'A':
	    edit_path_change++;
	    edit_path = APPLICABLE;
	    break;

	case 'V':		
	    printf( "%s\n", version );
	    printf( "%s\n", checksumlist );
	    exit( 0 );

	case '?':
	    printf( "bad %c\n", c );
	    errflag++;
	    break;

	default: 
	    break;
	}
    }

    if (( finish != 0 ) && ( !use_outfile )) {
	errflag++;
    }
    if (( edit_path == APPLICABLE ) && ( skip )) {
	errflag++;
    }
    if ( edit_path_change > 1 ) {
	errflag++;
    }

    /* Check that kfile isn't an abvious directory */
    len = strlen( kfile );
    if ( kfile[ len - 1 ] == '/' ) {
        errflag++;
    }

    if ( errflag || ( argc - optind != 1 )) {
	fprintf( stderr, "usage: %s { -C | -A | -1 } [ -V ] ", argv[ 0 ] );
	fprintf( stderr, "[ -K command ] " );
	fprintf( stderr, "[ -c checksum ] [ -o file [ -%% ] ] path\n" );
	exit ( 2 );
    }

    /* verify the path we've been given exists */
    if ( stat( argv[ optind ], &st ) != 0 ) {
	perror( argv[ optind ] );
	exit( 2 );
    }

    /* initialize the transcripts */
    transcript_init( kfile, gotkfile );

    fs_walk( argv[ optind ], 0, finish );

    if ( finish > 0 ) {
	printf( "%%%d\n", ( int )finish );
    }

    /* free the transcripts */
    transcript_free( );
    hardlink_free( );

    /* close the output file */     
    fclose( outtran );

    exit( 0 );	
}
