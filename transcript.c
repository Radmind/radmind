/*
 * Copyright (c) 2003 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/param.h>
#ifdef sun
#include <sys/mkdev.h>
#endif /* sun */
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "applefile.h"
#include "base64.h"
#include "transcript.h"
#include "argcargv.h"
#include "code.h"
#include "radstat.h"
#include "cksum.h"
#include "pathcmp.h"
#include "largefile.h"
#include "list.h"

int read_kfile( char *kfile, int location );

struct transcript		*tran_head = NULL;
static struct transcript	*prev_tran = NULL;
extern int			edit_path;
static int			foundspecial = 0;
static char			*kdir;
static struct list		*kfile_list;
struct list			*special_list;

char				*path_prefix = NULL;
int				edit_path;
int				skip;
int				cksum;
FILE				*outtran;

    void 
transcript_parse( struct transcript *tran ) 
{
    char			line[ 2 * MAXPATHLEN ];
    int				length;
    char			*epath;
    char			**argv;
    int				ac;

    /* read in the next line in the transcript, loop through blanks and # */
    do {
	if (( fgets( line, MAXPATHLEN, tran->t_in )) == NULL ) {
	    tran->t_eof = 1;
	    return;
	}
	tran->t_linenum++;

	/* check to see if line contains the whole line */
	length = strlen( line );
	if ( line[ length - 1 ] != '\n' ) {
	    fprintf( stderr, "%s: line %d: line too long\n",
		    tran->t_fullname, tran->t_linenum );
	    exit( 2 );
	} 
    } while ((( ac = argcargv( line, &argv )) == 0 ) || ( *argv[ 0 ] == '#' ));

    if ( ac < 3 ) {
	fprintf( stderr, "%s: line %d: minimum 3 arguments, got %d\n",
		tran->t_fullname, tran->t_linenum, ac );
	exit( 2 );
    }

    if ( strlen( argv[ 0 ] ) != 1 ) {
	fprintf( stderr, "%s: line %d: %s is too long to be a type\n",
		tran->t_fullname, tran->t_linenum, argv[ 0 ] );
	exit( 2 );
    }

    if ( argv[ 0 ][ 0 ] == '-' ) {
	argv++;
	ac--;
	tran->t_pinfo.pi_minus = 1;
    } else {
	tran->t_pinfo.pi_minus = 0;
    }
    if ( argv[ 0 ][ 0 ] == '+' ) {
	argv++;
	ac--;
    }

    tran->t_pinfo.pi_type = argv[ 0 ][ 0 ];
    if (( epath = decode( argv[ 1 ] )) == NULL ) {
	fprintf( stderr, "%s: line %d: path too long\n",
	    tran->t_fullname, tran->t_linenum );
	exit( 2 );
    }
    if ( pathcmp( epath, tran->t_pinfo.pi_name ) < 0 ) {
	fprintf( stderr, "%s: line %d: bad sort order\n",
	    tran->t_fullname, tran->t_linenum );
	exit( 2 );
    }

    strcpy( tran->t_pinfo.pi_name, epath );

    /* reading and parsing the line */
    switch( *argv[ 0 ] ) {
    case 'd':				    /* dir */
#ifdef __APPLE__
	if (( ac != 5 ) && ( ac != 6 )) {
	    fprintf( stderr, "%s: line %d: expected 5 or 6 arguments, got %d\n",
		    tran->t_fullname, tran->t_linenum, ac );
	    exit( 2 );
	}
#else /* !__APPLE__ */
	if ( ac != 5 ) {
	    fprintf( stderr, "%s: line %d: expected 5 arguments, got %d\n",
		    tran->t_fullname, tran->t_linenum, ac );
	    exit( 2 );
	}
#endif /* __APPLE__ */

	tran->t_pinfo.pi_stat.st_mode = strtol( argv[ 2 ], NULL, 8 );
	tran->t_pinfo.pi_stat.st_uid = atoi( argv[ 3 ] );
	tran->t_pinfo.pi_stat.st_gid = atoi( argv[ 4 ] );
	if ( ac == 6 ) {
	    base64_d( argv[ 5 ], strlen( argv[ 5 ] ),
		    tran->t_pinfo.pi_afinfo.ai.ai_data );
	} else {
	    memset( tran->t_pinfo.pi_afinfo.ai.ai_data, 0, FINFOLEN );
	}
	break;

    case 'p':
    case 'D':
    case 's':
	if ( ac != 5 ) {
	    fprintf( stderr, "%s: line %d: expected 5 arguments, got %d\n",
		    tran->t_fullname, tran->t_linenum, ac );
	    exit( 2 );
	}
	tran->t_pinfo.pi_stat.st_mode = strtol( argv[ 2 ], NULL, 8 );
	tran->t_pinfo.pi_stat.st_uid = atoi( argv[ 3 ] );
	tran->t_pinfo.pi_stat.st_gid = atoi( argv[ 4 ] );
	break;

    case 'b':				    /* block or char */
    case 'c':
	if ( ac != 7 ) {
	    fprintf( stderr, "%s: line %d: expected 7 arguments, got %d\n",
		    tran->t_fullname, tran->t_linenum, ac );
	    exit( 2 );
	}
	tran->t_pinfo.pi_stat.st_mode = strtol( argv[ 2 ], NULL, 8 );
	tran->t_pinfo.pi_stat.st_uid = atoi( argv[ 3 ] );
	tran->t_pinfo.pi_stat.st_gid = atoi( argv[ 4 ] );
	tran->t_pinfo.pi_stat.st_rdev =
		makedev( ( unsigned )( atoi( argv[ 5 ] )), 
		( unsigned )( atoi( argv[ 6 ] )));
	break;

    case 'l':				    /* link */
    case 'h':				    /* hard */
	if ( ac != 3 ) {
	    fprintf( stderr, "%s: line %d: expected 3 arguments, got %d\n",
		    tran->t_fullname, tran->t_linenum, ac );
	    exit( 2 );
	}
	if (( epath = decode( argv[ 2 ] )) == NULL ) {
	    fprintf( stderr, "%s: line %d: target path too long\n",
		tran->t_fullname, tran->t_linenum );
	    exit( 2 );
	}
	strcpy( tran->t_pinfo.pi_link, epath );
	break;

    case 'a':				    /* hfs applefile */
    case 'f':				    /* file */
	if ( ac != 8 ) {
	    fprintf( stderr, "%s: line %d: expected 8 arguments, got %d\n",
		    tran->t_fullname, tran->t_linenum, ac );
	    exit( 2 );
	}
	tran->t_pinfo.pi_stat.st_mode = strtol( argv[ 2 ], NULL, 8 );
	tran->t_pinfo.pi_stat.st_uid = atoi( argv[ 3 ] );
	tran->t_pinfo.pi_stat.st_gid = atoi( argv[ 4 ] );
	tran->t_pinfo.pi_stat.st_mtime = atoi( argv[ 5 ] );
	tran->t_pinfo.pi_stat.st_size = strtoofft( argv[ 6 ], NULL, 10 );
	if ( tran->t_type != T_NEGATIVE ) {
	    if (( cksum ) && ( strcmp( "-", argv [ 7 ] ) == 0  )) {
		fprintf( stderr, "%s: line %d: no cksums in transcript\n",
			tran->t_fullname, tran->t_linenum );
		exit( 2 );
	    }
	}
	strcpy( tran->t_pinfo.pi_cksum_b64, argv[ 7 ] );
	break;

    default:
	fprintf( stderr,
	    "%s: line %d: unknown file type '%c'\n",
	    tran->t_fullname, tran->t_linenum, *argv[ 0 ] );
	exit( 2 );
    }

    return;
}

    void
t_print( struct pathinfo *fs, struct transcript *tran, int flag ) 
{
    struct pathinfo	*cur;
    char		*epath;
    dev_t		dev;

#ifdef __APPLE__
    static char         null_buf[ 32 ] = { 0 };
#endif /* __APPLE__ */

    if ( edit_path == APPLICABLE ) {
	cur = &tran->t_pinfo;
    } else {
	cur = fs;	/* What if this is NULL? */
    }

    /* Print name of transcript if it changed since the last t_print */
    if (( edit_path == APPLICABLE )
	    && (( flag == PR_TRAN_ONLY ) || ( flag == PR_DOWNLOAD )
		|| ( flag == PR_STATUS_NEG ))
	    && ( prev_tran != tran )) {
	fprintf( outtran, "%s:\n", tran->t_shortname );
	prev_tran = tran;
    }

    /*
     * If a file is missing from the edit_path that was chosen, a - is 
     * printed and then the file name that is missing is printed.
     */
    if ( edit_path == APPLICABLE ) {
	if ( flag == PR_FS_ONLY ) {
	    fprintf( outtran, "- " );
	    cur = fs;
	} else if ( flag == PR_STATUS_MINUS ) {
	    fprintf( outtran, "- " );
	}
    } else if (( edit_path ==  CREATABLE ) &&
	    (( flag == PR_TRAN_ONLY ) || ( fs->pi_type == 'X' ))) {
	fprintf( outtran, "- " );
	cur = &tran->t_pinfo;
    } 

    if (( epath = encode( cur->pi_name )) == NULL ) {
	fprintf( stderr, "Filename too long: %s\n", cur->pi_name );
	exit( 2 );
    }

    /* print out info to file based on type */
    switch( cur->pi_type ) {
    case 's':
    case 'D':
    case 'p':
	fprintf( outtran, "%c %-37s\t%.4lo %5d %5d\n", cur->pi_type, epath, 
		(unsigned long )( T_MODE & cur->pi_stat.st_mode ), 
		(int)cur->pi_stat.st_uid, (int)cur->pi_stat.st_gid );
	break;

    case 'd':
#ifdef __APPLE__
	if ( memcmp( cur->pi_afinfo.ai.ai_data, null_buf,
		sizeof( null_buf )) != 0 ) { 
	    char	finfo_e[ SZ_BASE64_E( FINFOLEN ) ];

	    base64_e( cur->pi_afinfo.ai.ai_data, FINFOLEN, finfo_e );
	    fprintf( outtran, "%c %-37s\t%.4lo %5d %5d %s\n", cur->pi_type,
		    epath,
		    (unsigned long)( T_MODE & cur->pi_stat.st_mode ), 
		    (int)cur->pi_stat.st_uid, (int)cur->pi_stat.st_gid,
		    finfo_e );
	    break;
	}
#endif /* __APPLE__ */
	fprintf( outtran, "%c %-37s\t%.4lo %5d %5d\n", cur->pi_type, epath, 
		(unsigned long )( T_MODE & cur->pi_stat.st_mode ), 
		(int)cur->pi_stat.st_uid, (int)cur->pi_stat.st_gid );
	break;

    case 'l':
    case 'h':
	fprintf( outtran, "%c %-37s\t", cur->pi_type, epath );
	if (( epath = encode( cur->pi_link )) == NULL ) {
	    fprintf( stderr, "Filename too long: %s\n", cur->pi_link );
	    exit( 2 );
	}
	fprintf( outtran, "%s\n", epath );
	break;

    case 'a':		/* hfs applesingle file */
    case 'f':
	if (( edit_path == APPLICABLE ) && (( flag == PR_TRAN_ONLY ) || 
		( flag == PR_DOWNLOAD ))) {
	    fprintf( outtran, "+ " );
	}

	/*
	 * PR_STATUS_NEG means we've had a permission change on a file,
	 * but the corresponding transcript is negative, hence, retain
	 * the file system's mtime.  Woof!
	 */
	fprintf( outtran, "%c %-37s\t%.4lo %5d %5d %9d %7" PRIofft "d %s\n",
		cur->pi_type, epath,
		(unsigned long)( T_MODE & cur->pi_stat.st_mode ), 
		(int)cur->pi_stat.st_uid, (int)cur->pi_stat.st_gid,
		( flag == PR_STATUS_NEG ) ?
			(int)fs->pi_stat.st_mtime : (int)cur->pi_stat.st_mtime,
		cur->pi_stat.st_size, cur->pi_cksum_b64 );
	break;

    case 'c':
    case 'b':
	dev = cur->pi_stat.st_rdev;
	fprintf( outtran, "%c %-37s\t%.4lo %5d %5d %5d %5d\n",
		cur->pi_type, epath,
		(unsigned long )( T_MODE & cur->pi_stat.st_mode ), 
		(int)cur->pi_stat.st_uid, (int)cur->pi_stat.st_gid,
		(int)major(dev), (int)minor(dev) );
	break;

    case 'X' :
	perror( cur->pi_name );
	exit( 2 );

    default:
	fprintf( stderr, "PANIC! XXX OOPS!\n" );
	exit( 2 );
    } 
}

   static int 
t_compare( struct pathinfo *fs, struct transcript *tran )
{
    int			cmp;
    mode_t		mode;
    mode_t		tran_mode;
    dev_t		dev;

    /*
     * If the transcript is at EOF, and we've exhausted the filesystem,
     * just return T_MOVE_FS, as this will cause transcript() to return.
     */
    if (( tran->t_eof ) && ( fs == NULL )) {
	return T_MOVE_FS;
    }

    if ( tran->t_eof ) {
	cmp = -1;
    } else {
	if ( fs == NULL ) {
	    /*
	     * If we've exhausted the filesystem, cmp = 1 means that
	     * name is in tran, but not fs.
	     */
	    cmp = 1;
	} else {
	    cmp = pathcmp( fs->pi_name, tran->t_pinfo.pi_name );
	}
    }

    if ( cmp > 0 ) {
	/* name is in the tran, but not the fs */
	t_print( fs, tran, PR_TRAN_ONLY ); 
	return T_MOVE_TRAN;
    } 

    /*
     * after this point, name is in the fs, so if it's 'f', and
     * checksums are on, get the checksum
     */
    if ( cksum ) {
	if ( fs->pi_type == 'f' ) {
	    if ( do_cksum( fs->pi_name, fs->pi_cksum_b64 ) < 0 ) {
		perror( fs->pi_name );
		exit( 2 );
	    }
	} else if ( fs->pi_type == 'a' ) {
	    if ( do_acksum( fs->pi_name, fs->pi_cksum_b64,
	    	    &fs->pi_afinfo ) < 0 ) {
		perror( fs->pi_name );
		exit( 2 );
	    }
	}
    }

    if ( cmp < 0 ) {
	/* name is not in the tran */
	t_print( fs, tran, PR_FS_ONLY );
	return T_MOVE_FS;
    } 

    /* convert the modes */
    mode = ( T_MODE & fs->pi_stat.st_mode );
    tran_mode = ( T_MODE & tran->t_pinfo.pi_stat.st_mode );

    /* the names match so check types */
    if ( fs->pi_type != tran->t_pinfo.pi_type ) {
	t_print( fs, tran, PR_DOWNLOAD );
	return T_MOVE_BOTH;
    }

    /* compare the other components for each file type */
    switch( fs->pi_type ) {
    case 'a':			    /* hfs applefile */
    case 'f':			    /* file */
	if ( tran->t_type != T_NEGATIVE ) {
	    if (( fs->pi_stat.st_size != tran->t_pinfo.pi_stat.st_size ) ||
(( !cksum ) ? ( fs->pi_stat.st_mtime != tran->t_pinfo.pi_stat.st_mtime ) :
( strcmp( fs->pi_cksum_b64, tran->t_pinfo.pi_cksum_b64 ) != 0 ))) {
		t_print( fs, tran, PR_DOWNLOAD );
		if (( edit_path == APPLICABLE ) && ( fs->pi_stat.st_nlink > 1 )) {
		    hardlink_changed( fs, 1 );
		}
		break;
	    }

	    if ( fs->pi_stat.st_mtime != tran->t_pinfo.pi_stat.st_mtime ) {
		t_print( fs, tran, PR_STATUS );
		break;
	    }
	}

	if (( fs->pi_stat.st_uid != tran->t_pinfo.pi_stat.st_uid ) || 
		( fs->pi_stat.st_gid != tran->t_pinfo.pi_stat.st_gid ) ||
		( mode != tran_mode )) {
	    if (( tran->t_type == T_NEGATIVE ) && ( edit_path == APPLICABLE )) {
		t_print( fs, tran, PR_STATUS_NEG );
	    } else {
		t_print( fs, tran, PR_STATUS );
	    }
	}
	break;

    case 'd':				/* dir */
#ifdef __APPLE__
	if ( tran->t_type != T_NEGATIVE ) {
	    if (( fs->pi_stat.st_uid != tran->t_pinfo.pi_stat.st_uid ) ||
		    ( fs->pi_stat.st_gid != tran->t_pinfo.pi_stat.st_gid ) ||
		    ( memcmp( fs->pi_afinfo.ai.ai_data,
		    tran->t_pinfo.pi_afinfo.ai.ai_data, FINFOLEN ) != 0 ) ||
		    ( mode != tran_mode )) {
		t_print( fs, tran, PR_STATUS );
	    }
	    break;
	}
#endif /* __APPLE__ */
	if (( fs->pi_stat.st_uid != tran->t_pinfo.pi_stat.st_uid ) ||
		( fs->pi_stat.st_gid != tran->t_pinfo.pi_stat.st_gid ) ||
		( mode != tran_mode )) {
	    t_print( fs, tran, PR_STATUS );
	}
	break;

    case 'D':
    case 'p':
    case 's':
	if (( fs->pi_stat.st_uid != tran->t_pinfo.pi_stat.st_uid ) ||
		( fs->pi_stat.st_gid != tran->t_pinfo.pi_stat.st_gid ) ||
		( mode != tran_mode )) {
	    t_print( fs, tran, PR_STATUS );
	}
	break;

    case 'l':			    /* link */
	if ( tran->t_type != T_NEGATIVE ) {
	    if ( strcmp( fs->pi_link, tran->t_pinfo.pi_link ) != 0 ) {
		t_print( fs, tran, PR_STATUS );
	    }
	}
	break;

    case 'h':			    /* hard */
	if (( strcmp( fs->pi_link, tran->t_pinfo.pi_link ) != 0 ) ||
		( hardlink_changed( fs, 0 ) != 0 )) {
	    t_print( fs, tran, PR_STATUS );
	}
	break;

    case 'c':
	/*
	 * negative character special files only check major and minor
	 * devices numbers. pseudo ttys can change uid, gid and mode for
	 * every login and this is normal behavior.
	 */
	dev = fs->pi_stat.st_rdev;
	if ( tran->t_type != T_NEGATIVE ) {
	    if (( fs->pi_stat.st_uid != tran->t_pinfo.pi_stat.st_uid ) ||
		    ( fs->pi_stat.st_gid != tran->t_pinfo.pi_stat.st_gid ) || 
		    ( mode != tran_mode )) {
		t_print( fs, tran, PR_STATUS );
	    }
	}
	if ( dev != tran->t_pinfo.pi_stat.st_rdev ) {
	    t_print( fs, tran, PR_STATUS );
	}	
	break;

    case 'b':
	dev = fs->pi_stat.st_rdev;
	if (( fs->pi_stat.st_uid != tran->t_pinfo.pi_stat.st_uid ) ||
		( fs->pi_stat.st_gid != tran->t_pinfo.pi_stat.st_gid ) || 
		( dev != tran->t_pinfo.pi_stat.st_rdev ) ||
		( mode != tran_mode )) {
	    t_print( fs, tran, PR_STATUS );
	}	
	break;

    default:
	fprintf( stderr, "OOPS! XXX DOUBLE PANIC!\n" );
	break;
    }

    return T_MOVE_BOTH;
}

/* 
 * Loop through the list of transcripts and compare each
 * to find which transcript to start with. Only switch to the
 * transcript if it is not at EOF.  A transcript at EOF may
 * still be returned.
 */
    struct transcript *
transcript_select( void )
{
    struct transcript	*next_tran = NULL;
    struct transcript	*begin_tran = NULL;

    for (;;) {
	for ( begin_tran = tran_head, next_tran = tran_head->t_next;
		next_tran != NULL; next_tran = next_tran->t_next ) {
	    if ( begin_tran->t_eof ) {
		begin_tran = next_tran;
		continue;
	    }
	    if ( ! next_tran->t_eof ) {
		if ( pathcmp( next_tran->t_pinfo.pi_name,
			begin_tran->t_pinfo.pi_name ) < 0 ) {
		    begin_tran = next_tran;
		}
	    }
	}

	/* move ahead other transcripts that match */
	for ( next_tran = begin_tran->t_next; next_tran != NULL;
		next_tran = next_tran->t_next ) {
	    if ( pathcmp( begin_tran->t_pinfo.pi_name,
		    next_tran->t_pinfo.pi_name ) == 0 ) {
		transcript_parse( next_tran );
	    }
	}

	/* This is presumably the NULL transcript. */
	if ( begin_tran->t_eof ) {
	    return( begin_tran );
	}

	/*
	 * If the highest precedence transcript line has a leading '-',
	 * then just pretend it's not there.
	 */
	if ( begin_tran->t_pinfo.pi_minus ) {
	    continue;
	}

	/* Don't look outside of the initial path. */
	if ( !ischild( begin_tran->t_pinfo.pi_name, path_prefix )) {
	    continue;
	}

	transcript_parse( begin_tran );
    }
}

    int
transcript( char *path )
{
    struct pathinfo	pi;
    int			enter = 0;
    int 		len;
    char		epath[ MAXPATHLEN ];
    char		*linkpath;
    struct transcript	*tran = NULL;

    /*
     * path is NULL when we've been called after the filesystem has been
     * exhausted, to consume any remaining transcripts.
     */
    if ( path != NULL ) {
	switch ( radstat( path, &pi.pi_stat, &pi.pi_type,
		&pi.pi_afinfo )) {
	case 0:
	    break;
	case 1:
	    fprintf( stderr, "%s is of an unknown type\n", path );
	    exit( 2 );
	default:
	    if (( errno == ENOTDIR ) || ( errno == ENOENT )) {
		memset( &pi.pi_stat, 0, sizeof( struct stat ));
		pi.pi_type = 'X';
		break;
	    } else {
		perror( path );
		exit( 2 );
	    }
	}

	strcpy( pi.pi_name, path );

	/* if it's multiply referenced, check if it's a hardlink */
	if ( !S_ISDIR( pi.pi_stat.st_mode ) && ( pi.pi_stat.st_nlink > 1 ) &&
		(( linkpath = hardlink( &pi )) != NULL )) {
	    pi.pi_type = 'h';
	    strcpy( pi.pi_link, linkpath );
	} else if ( S_ISLNK( pi.pi_stat.st_mode )) {
	    len = readlink( pi.pi_name, epath, MAXPATHLEN );
	    epath[ len ] = '\0';
	    strcpy( pi.pi_link, epath );
	}

	/* By default, go into directories */
	if ( S_ISDIR( pi.pi_stat.st_mode )) {
	    enter = 1;
	} else { 
	    enter = 0;
	}

	/* initialize cksum field. */
	strcpy( pi.pi_cksum_b64, "-" );
    }

    for (;;) {
	tran = transcript_select();

	switch ( t_compare(( path ? &pi : NULL ), tran )) {
	case T_MOVE_FS :
	    return( enter );

	case T_MOVE_BOTH :
	    /* But don't go into negative directories */
	    if (( tran->t_type == T_NEGATIVE ) &&
		    ( tran->t_pinfo.pi_type == 'd' )) {
		enter = 2;
	    }
	    transcript_parse( tran );
	    return( enter );

	case T_MOVE_TRAN :
	    transcript_parse( tran );
	    break;

	default :
	    fprintf( stderr, "t_compare returned an unexpected value!\n" );
	    exit( 2 );
	}
    }
}

    static void
t_new( int type, char *fullname, char *shortname, char *kfile ) 
{
    struct transcript	 *new;

    if (( new = (struct transcript *)malloc( sizeof( struct transcript )))
	    == NULL ) {
	perror( "malloc" );
	exit( 2 );
    }
    memset( new, 0, sizeof( struct transcript ));

    new->t_type = type;
    if ( new->t_type == T_NULL ) {
	new->t_eof = 1; 

    } else {
	new->t_eof = 0; 
	new->t_linenum = 0;
	strcpy( new->t_shortname, shortname );
	strcpy( new->t_fullname, fullname );
	strcpy( new->t_kfile, kfile );
	if (( new->t_in = fopen( fullname, "r" )) == NULL ) {
	    perror( fullname );
	    exit( 2 );
	}
	transcript_parse( new );
    }

    new->t_next = tran_head;
    tran_head = new;

    return;
}

    void
transcript_init( char *kfile, int location )
{
    char	*special = "special.T";
    char	*p;
    char	fullpath[ MAXPATHLEN ];

    /*
     * Make sure that there's always a transcript to read, so other code
     * doesn't have to check it.
     */
    t_new( T_NULL, NULL, NULL, NULL );

    if ( skip ) {
	return;
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
    if (( kfile_list = list_new( )) == NULL ) {
	perror( "list_new" );
	exit( 2 );
    }
    if ( list_insert( kfile_list, kfile ) != 0 ) {
	perror( "list_insert" );
	exit( 2 );
    }
    if (( special_list = list_new( )) == NULL ) {
	perror( "list_new" );
	exit( 2 );
    }
    if ( read_kfile( kfile, location ) != 0 ) {
	exit( 2 );
    }

    if ( foundspecial && ( location == K_CLIENT )) {
	/* open the special transcript if there were any special files */
	if ( strlen( kdir ) + strlen( special ) + 2 > MAXPATHLEN ) {
	    fprintf( stderr, 
		    "special path too long: %s%s\n", kdir, special );
	    exit( 2 );
	}
	sprintf( fullpath, "%s%s", kdir, special );
	t_new( T_SPECIAL, fullpath, special, "special" );
    }

    if ( tran_head->t_type == T_NULL  && edit_path == APPLICABLE ) {
	fprintf( stderr, "-A option requires a non-NULL transcript\n" );
	exit( 2 );
    }

    return;
}

    int
read_kfile( char *kfile, int location )
{
    int		length, ac, linenum = 0;
    char	line[ MAXPATHLEN ];
    char	fullpath[ MAXPATHLEN ];
    char	*subpath;
    char	**av;
    FILE	*fp;

    if (( fp = fopen( kfile, "r" )) == NULL ) {
	perror( kfile );
	return( -1 );
    }

    while ( fgets( line, sizeof( line ), fp ) != NULL ) {
	linenum++;
	length = strlen( line );
	if ( line[ length - 1 ] != '\n' ) {
	    fprintf( stderr, "command: line %d: line too long\n", linenum );
	    return( -1 );
	}

	/* skips blank lines and comments */
	if ((( ac = argcargv( line, &av )) == 0 ) || ( *av[ 0 ] == '#' )) {
	    continue;
	}

	if ( ac != 2 ) {
	    fprintf( stderr, "command: line %d: expected 2 arguments, got %d\n",
		    linenum, ac );
	    return( -1 );
	} 

	switch( location ) {
	case K_CLIENT:
	    if ( snprintf( fullpath, MAXPATHLEN, "%s%s", kdir,
		    av[ 1 ] ) >= MAXPATHLEN ) {
		fprintf( stderr, "command: line %d: path too long\n",
			linenum );
		fprintf( stderr, "command: line %d: %s%s\n",
			linenum, kdir, av[ 1 ] );
		return( -1 );
	    }
	    break;

	case K_SERVER:
	    if ( *av[ 0 ] == 'k' ) {
		subpath = "command";
	    } else {
		subpath = "transcript";
	    }
	    if ( snprintf( fullpath, MAXPATHLEN, "%s/%s/%s", _RADMIND_PATH,
			subpath, av[ 1 ] ) >= MAXPATHLEN ) {
		fprintf( stderr, "command: line %d: path too long\n",
			linenum );
		fprintf( stderr, "command: line %d: %s%s\n",
			linenum, kdir, av[ 1 ] );
		return( -1 );
	    }
	    break;

	default:
	    fprintf( stderr, "unknown location\n" );
	    return( -1 );
	}

	switch( *av[ 0 ] ) {
	case 'k':				/* command file */
	    if ( list_check( kfile_list, fullpath )) {
		fprintf( stderr,
		    "%s: line %d: command file loop: %s already included\n",
		    kfile, linenum, av[ 1 ] );
		return( -1 );
	    }
	    if ( list_insert( kfile_list, fullpath ) != 0 ) {
		perror( "list_insert" );
		return( -1 );
	    }

	    if ( read_kfile( fullpath, location ) != 0 ) {
		return( -1 );
	    }
	    break;

	case 'n':				/* negative */
	    t_new( T_NEGATIVE, fullpath, av[ 1 ], kfile );
	    break;

	case 'p':				/* positive */
	    t_new( T_POSITIVE, fullpath, av[ 1 ], kfile );
	    break;

	case 's':				/* special */
	    foundspecial++;
	    if ( location == K_SERVER ) {
		if ( list_insert( special_list, av[ 1 ] ) != 0 ) {
		    perror( "list_insert" );
		    return( -1 );
		}
	    }
	    continue;

	default:
	    fprintf( stderr, "command: line %d: '%s' invalid\n",
		    linenum, av[ 0 ] );
	    return( -1 );
	}
    }

    if ( fclose( fp ) != 0 ) {
	perror( kfile );
	return( -1 );
    }

    return( 0 );
}

    void
transcript_free( )
{
    struct transcript	 *next;

    /*
     * Call transcript() with NULL to indicate that we've run out of
     * filesystem to compare against.
     */
    transcript( NULL );

    while ( tran_head != NULL ) {
	next = tran_head->t_next;
	if ( tran_head->t_in != NULL ) {
	    fclose( tran_head->t_in );
	}
	free( tran_head );
	tran_head = next;
    }
}
