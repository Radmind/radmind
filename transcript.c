/*
 * Copyright (c) 2003 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#include <sys/types.h>
#include <sys/param.h>
#ifdef sun
#include <sys/mkdev.h>
#endif /* sun */
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

static struct transcript	*tran_head = NULL;
static struct transcript	*prev_tran = NULL;
extern int			edit_path;

    static void 
t_parse( struct transcript *tran ) 
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
    epath = decode( argv[ 1 ] );
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
		    tran->t_pinfo.pi_afinfo.fi.fi_data );
	} else {
	    memset( tran->t_pinfo.pi_afinfo.fi.fi_data, 0, FINFOLEN );
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
	epath = decode( argv[ 2 ] );
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
	tran->t_pinfo.pi_stat.st_size = atoi( argv[ 6 ] );
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

    static void
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

    /*
     * If a file is missing from the edit_path that was chosen, a - is 
     * printed and then the file name that is missing is printed.
     */
    if (( edit_path == APPLICABLE ) && ( flag == PR_FS_ONLY )) {
	fprintf( outtran, "- " );
	cur = fs;
    } else if (( edit_path ==  CREATABLE ) && ( flag == PR_TRAN_ONLY )) {
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
	if ( memcmp( cur->pi_afinfo.fi.fi_data, null_buf,
		sizeof( null_buf )) != 0 ) { 
	    char	finfo_e[ SZ_BASE64_E( FINFOLEN ) ];

	    base64_e( cur->pi_afinfo.fi.fi_data, FINFOLEN, finfo_e );
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
	    if ( prev_tran != tran ) {
		fprintf( outtran, "%s:\n", tran->t_shortname );
		prev_tran = tran;
	    }
	    fprintf( outtran, "+ " );
	}

	/*
	 * PR_STATUS_NEG means we've had a permission change on a file,
	 * but the corresponding transcript is negative, hence, retain
	 * the file system's mtime.  Woof!
	 */
	fprintf( outtran, "%c %-37s\t%.4lo %5d %5d %9d %7d %s\n",
		cur->pi_type, epath,
		(unsigned long)( T_MODE & cur->pi_stat.st_mode ), 
		(int)cur->pi_stat.st_uid, (int)cur->pi_stat.st_gid,
		( flag == PR_STATUS_NEG ) ?
			(int)fs->pi_stat.st_mtime : (int)cur->pi_stat.st_mtime,
		(int)cur->pi_stat.st_size, cur->pi_cksum_b64 );
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
	/*
	 * If the highest precedence transcript line has a leading '-',
	 * then just pretend it's not there.
	 */
	if ( tran->t_pinfo.pi_minus ) {
	    return T_MOVE_TRAN;
	}

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
	    	    &fs->pi_afinfo )
		    < 0 ) {
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
		    ( memcmp( fs->pi_afinfo.fi.fi_data,
		    tran->t_pinfo.pi_afinfo.fi.fi_data, FINFOLEN ) != 0 ) ||
		    ( mode != tran_mode )) {
		t_print( fs, tran, PR_STATUS );
	    }
	}
	break;
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

    int
transcript( struct pathinfo *new )
{

    int			enter; 
    int 		len;
    char		epath[ MAXPATHLEN ];
    char		*path;
    struct transcript	*next_tran = NULL;
    struct transcript	*begin_tran = NULL;

    /*
     * new is NULL when we've been called after the filesystem has been
     * exhausted, to consume any remaining transcripts.
     */
    if ( new != NULL ) {
	switch ( radstat( new->pi_name, &new->pi_stat, &new->pi_type,
		&new->pi_afinfo )) {
	case 0:
	    break;
	case 1:
	    fprintf( stderr, "%s is of an unknown type\n", new->pi_name );
	    exit( 2 );
	default:
	    perror( new->pi_name );
	    exit( 2 );
	}

	/* if it's multiply referenced, check if it's a hardlink */
	if ( !S_ISDIR( new->pi_stat.st_mode ) &&
		( new->pi_stat.st_nlink > 1 ) &&
		(( path = hardlink( new )) != NULL )) {
	    new->pi_type = 'h';
	    strcpy( new->pi_link, path );
	} else if ( S_ISLNK( new->pi_stat.st_mode )) {
	    len = readlink( new->pi_name, epath, MAXPATHLEN );
	    epath[ len ] = '\0';
	    strcpy( new->pi_link, epath );
	}

	/* By default, go into directories */
	if ( S_ISDIR( new->pi_stat.st_mode )) {
	    enter = 1;
	} else { 
	    enter = 0;
	}

	/* initialize cksum field. */
	strcpy( new->pi_cksum_b64, "-" );
    }

    for (;;) {
	/* 
	 * Loop through the list of transcripts and compare each
	 * to find which transcript to start with. Only switch to the
	 * transcript if it is not at EOF.
	 */
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
		t_parse( next_tran );
	    }
	}

	switch ( t_compare( new, begin_tran )) {
	case T_MOVE_FS :
	    return( enter );

	case T_MOVE_BOTH :
	    /* But don't go into negative directories */
	    if (( begin_tran->t_type == T_NEGATIVE ) &&
		    ( begin_tran->t_pinfo.pi_type == 'd' )) {
		enter = 0;
	    }
	    t_parse( begin_tran );
	    return( enter );

	case T_MOVE_TRAN :
	    t_parse( begin_tran );
	    break;

	default :
	    fprintf( stderr, "OOPS! XXX FAMINE and DESPAIR!\n" );
	    exit( 2 );
	}
    }
}

    static void
t_new( int type, char *fullname, char *shortname ) 
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
	if (( new->t_in = fopen( fullname, "r" )) == NULL ) {
	    perror( fullname );
	    exit( 2 );
	}
	t_parse( new );
    }

    new->t_next = tran_head;
    tran_head = new;

    return;
}

    void
transcript_init( char *kfile, int kfilemustexist )
{
    char	**av;
    char	*special = "special.T";
    char	line[ MAXPATHLEN ];
    char	fullpath[ MAXPATHLEN ];
    char	*p;
    int		length, ac, linenum = 0;
    int		foundspecial = 0;
    FILE	*fp;
    char	*kdir;

    /*
     * Make sure that there's always a transcript to read, so other code
     * doesn't have to check it.
     */
    t_new( T_NULL, NULL, NULL );

    if ( skip ) {
	return;
    }

    if (( fp = fopen( kfile, "r" )) == NULL ) {
	if ( kfilemustexist || ( edit_path == APPLICABLE )) {
	    perror( kfile );
	    exit( 2 );
	}
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

    while ( fgets( line, sizeof( line ), fp ) != NULL ) {
	linenum++;
	length = strlen( line );
	if ( line[ length - 1 ] != '\n' ) {
	    fprintf( stderr, "command: line %d: line too long\n", linenum );
	    exit( 2 );
	}

	/* skips blank lines and comments */
	if ((( ac = argcargv( line, &av )) == 0 ) || ( *av[ 0 ] == '#' )) {
	    continue;
	}

	if ( ac != 2 ) {
	    fprintf( stderr, "command: line %d: expected 2 arguments, got %d\n",
		    linenum, ac );
	    exit( 2 );
	} 

	if ( strlen( kdir ) + strlen( av[ 1 ] ) + 2 > MAXPATHLEN ) {
	    fprintf( stderr, "command: line %d: transcript name too long\n",
		    linenum );
	    fprintf( stderr, "command: line %d: %s%s\n",
		    linenum, kdir, av[ 1 ] );
	    exit( 2 );
	}
	sprintf( fullpath, "%s%s", kdir, av[ 1 ] );

	switch( *av[ 0 ] ) {
	case 'p':				/* positive */
	    t_new( T_POSITIVE, fullpath, av[ 1 ] );
	    break;
	case 'n':				/* negative */
	    t_new( T_NEGATIVE, fullpath, av[ 1 ] );
	    break;
	case 's':				/* special */
	    foundspecial++;
	    continue;
	default:
	    fprintf( stderr, "command: line %d: '%s' invalid\n",
		    linenum, av[ 0 ] );
	    exit( 2 );
	}
    }

    fclose( fp );

    /* open the special transcript if there were any special files */
    if ( foundspecial ) {
	if ( strlen( kdir ) + strlen( special ) + 2 > MAXPATHLEN ) {
	    fprintf( stderr, 
		    "special path too long: %s%s\n", kdir, special );
	    exit( 2 );
	}
	sprintf( fullpath, "%s%s", kdir, special );
	t_new( T_SPECIAL, fullpath, special );
    }

    if ( tran_head->t_type == T_NULL  && edit_path == APPLICABLE ) {
	fprintf( stderr, "-A option requires a non-NULL transcript\n" );
	exit( 2 );
    }

    return;
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
