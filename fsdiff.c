/*
 * Copyright (c) 2003 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include "config.h"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>

#include <openssl/evp.h>

#include "applefile.h"
#include "transcript.h"
#include "pathcmp.h"
#include "radstat.h"

void            (*logger)( char * ) = NULL;

extern char	*version, *checksumlist;

void		fs_walk( char *, struct stat *, char *, struct applefileinfo *,
	int, int, int );
int		dodots = 0;
int		dotfd;
int		lastpercent = -1;
int		case_sensitive = 1;
int		tran_format = -1; 
extern int	exclude_warnings;
const EVP_MD    *md;

static struct fs_list *fs_insert( struct fs_list **, struct fs_list *,
	char *, int (*)( const char *, const char * ));

struct fs_list {
    struct fs_list		*fl_next;
    char			*fl_name;
    struct stat			fl_stat;
    char			fl_type;
    struct applefileinfo	fl_afinfo;
};

    static struct fs_list *
fs_insert( struct fs_list **head, struct fs_list *last,
	char *name, int (*cmp)( const char *, const char * ))
{
    struct fs_list	**current, *new;

    if (( last != NULL ) && ( (*cmp)( name, last->fl_name ) > 0 )) {
	current = &last->fl_next;
    } else {
	current = head;
    }

    /* find where in the list to put the new entry */
    for ( ; *current != NULL; current = &(*current)->fl_next) {
	if ( (*cmp)( name, (*current)->fl_name ) <= 0 ) {
	    break;
	}
    }

    if (( new = malloc( sizeof( struct fs_list ))) == NULL ) {
	return( NULL );
    }
    if (( new->fl_name = strdup( name )) == NULL ) {
	free( new );
	return( NULL );
    }

    new->fl_next = *current;
    *current = new; 
    return( new ); 
}

    void
fs_walk( char *path, struct stat *st, char *type, struct applefileinfo *afinfo,
	int start, int finish, int pdel ) 
{
    DIR			*dir;
    struct dirent	*de;
    struct fs_list	*head = NULL, *cur, *new = NULL, *next;
    int			len;
    int			count = 0;
    int			del_parent;
    float		chunk, f = start;
    char		temp[ MAXPATHLEN ];
    struct transcript	*tran;
    int			(*cmp)( const char *, const char * );

    if (( finish > 0 ) && ( start != lastpercent )) {
	lastpercent = start;
	printf( "%%%.2d %s\n", start, path );
	fflush( stdout );
    }

    /* call the transcript code */
    switch ( transcript( path, st, type, afinfo, pdel )) {
    case 2 :			/* negative directory */
	for (;;) {
	    tran = transcript_select();
	    if ( tran->t_eof ) {
		return;
	    }

	    if ( ischildcase( tran->t_pinfo.pi_name, path, case_sensitive )) {
		struct stat		st0;
		char			type0;
		struct applefileinfo	afinfo0;

		strcpy( temp, tran->t_pinfo.pi_name );
		switch ( radstat( temp, &st0, &type0, &afinfo0 )) {
		case 0:
		    break;
		case 1:
		    fprintf( stderr, "%s is of an unknown type\n", temp );
		    exit( 2 );
		default:
		    if (( errno != ENOTDIR ) && ( errno != ENOENT )) {
			perror( path );
			exit( 2 );
		    }
		}

		fs_walk( temp, &st0, &type0, &afinfo0, start, finish, pdel );
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
	fprintf( stderr, "transcript returned an unexpected value!\n" );
	exit( 2 );
    }

    /*
     * store whether object is to be deleted. if we get here, object
     * is a directory, which should mean that if fs_minus == 1 all
     * child objects should be removed as well. tracking this allows
     * us to zap excluded objects whose parent dir will be deleted.
     *
     * del_parent is passed into subsequent fs_walk and transcript
     * calls, where * it's checked when considering whether to
     * exclude an object.
     */
    del_parent = fs_minus;

    if ( case_sensitive ) {
	cmp = strcmp;
    } else {
	cmp = strcasecmp;
    }

    if ( chdir( path ) < 0 ) {
	perror( path );
	exit( 2 );
    }

    /* open directory */
    if (( dir = opendir( "." )) == NULL ) {
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

	if (( new = fs_insert( &head, new, de->d_name, cmp )) == NULL ) {
	    perror( "malloc" );
	    exit( 1 );
	}

	switch ( radstat( new->fl_name, &new->fl_stat, &new->fl_type,
		&new->fl_afinfo )) {
	case 0:
	    break;
	case 1:
	    fprintf( stderr, "%s is of an unknown type\n", path );
	    exit( 2 );
	default:
	    if (( errno != ENOTDIR ) && ( errno != ENOENT )) {
		perror( path );
		exit( 2 );
	    }
	}
    }

    if ( closedir( dir ) != 0 ) {
	perror( "closedir" );
	exit( 2 );
    }

    if ( fchdir( dotfd ) < 0 ) {
	perror( "OOPS!" );
	exit( 2 );
    }

    chunk = (( finish - start ) / ( float )count );

    len = strlen( path );

    /* call fswalk on each element in the sorted list */
    for ( cur = head; cur != NULL; cur = next ) {
	if ( path[ len - 1 ] == '/' ) {
	    if ( snprintf( temp, MAXPATHLEN, "%s%s", path, cur->fl_name )
		    >= MAXPATHLEN ) {
                fprintf( stderr, "%s%s: path too long\n", path, cur->fl_name );
		exit( 2 );
	    }
	} else {
            if ( snprintf( temp, MAXPATHLEN, "%s/%s", path, cur->fl_name )
		    >= MAXPATHLEN ) {
                fprintf( stderr, "%s/%s: path too long\n", path, cur->fl_name );
                exit( 2 );
            }
	}

	fs_walk( temp, &cur->fl_stat, &cur->fl_type, &cur->fl_afinfo,
		(int)f, (int)( f + chunk ), del_parent );

	f += chunk;

	next = cur->fl_next;
	free( cur->fl_name );
	free( cur );
    }

    return;
}

    int
main( int argc, char **argv ) 
{
    extern char 	*optarg;
    extern int		optind;
    char		*kfile = _RADMIND_COMMANDFILE;
    int 		c, len, edit_path_change = 0;
    int 		errflag = 0, use_outfile = 0;
    int			finish = 0;
    struct stat		st;
    char		type, buf[ MAXPATHLEN ];
    struct applefileinfo	afinfo;

    edit_path = CREATABLE;
    cksum = 0;
    outtran = stdout;

    while (( c = getopt( argc, argv, "%1ACc:IK:o:VvW" )) != EOF ) {
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

	case 'I':
	    case_sensitive = 0;
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

	case 'W':		/* print a warning when excluding an object */
	    exclude_warnings = 1;
	    break;

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
	fprintf( stderr, "usage: %s { -C | -A | -1 } [ -IVW ] ", argv[ 0 ] );
	fprintf( stderr, "[ -K command ] " );
	fprintf( stderr, "[ -c checksum ] [ -o file [ -%% ] ] path\n" );
	exit ( 2 );
    }

    path_prefix = argv[ optind ];
    len = strlen( path_prefix );

    /* Clip trailing '/' */
    if (( len > 1 ) && ( path_prefix[ len - 1 ] == '/' )) {
	path_prefix[ len - 1 ] = '\0';
	len--;
    }

    /* If path_prefix doesn't contain a directory, canonicalize it by
     * prepending a "./".  This allow paths to be dynamically converted between
     * relative and absolute paths without breaking sort order.
     */
    switch( path_prefix[ 0 ] ) {
    case '/':
        break;

    case '.':
	/* Don't rewrite '.' or paths starting with './' */
	if (( len == 1 ) || (  path_prefix[ 1 ] == '/' )) {
	    break;
	}
    default:
        if ( snprintf( buf, sizeof( buf ), "./%s",
                path_prefix ) >= MAXPATHLEN ) {
            fprintf( stderr, "path too long\n" );
            exit( 2 );
        }
	path_prefix = buf;
        break;
    }

    /* Determine if called with relative or absolute pathing.  Path is relative
     * if it's just '.' or starts with './'.  File names that start with a '.'
     * are absolute.
     */
    if ( path_prefix[ 0 ] == '.' ) {
	if ( len == 1 ) {
	    tran_format = T_RELATIVE;
	} else if ( path_prefix[ 1 ] == '/' ) {
	    tran_format = T_RELATIVE;
	} else {
	    tran_format = T_ABSOLUTE;
	}
    } else {
	tran_format = T_ABSOLUTE;
    }

    if ( radstat( path_prefix, &st, &type, &afinfo ) != 0 ) {
	perror( path_prefix );
	exit( 2 );
    }

    if (( dotfd = open( ".", O_RDONLY, 0 )) < 0 ) {
	perror( "OOPS!" );
	exit( 2 );
    }

    /* initialize the transcripts */
    transcript_init( kfile, K_CLIENT );

    fs_walk( path_prefix, &st, &type, &afinfo, 0, finish, 0 );

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
