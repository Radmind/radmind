#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#ifdef SOLARIS
#include <sys/mkdev.h>
#endif

#include <sys/stat.h>
#include <sys/param.h>
#include <unistd.h>

#include "transcript.h"
#include "llist.h"
#include "code.h"
#include "argcargv.h"

static struct transcript	*tran_head = NULL;
static struct transcript	*prev_tran = NULL;
static int			linenum = 0;

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
		    tran->t_name, tran->t_linenum );
	    exit( 1 );
	} 
    } while ((( ac = argcargv( line, &argv )) == 0 ) || ( *argv[ 0 ] == '#' ));

    if ( strlen( argv[ 0 ] ) != 1 ) {
	fprintf( stderr,
		"%s: line %d: %s is too long to be a type\n",
		tran->t_name, tran->t_linenum, argv[ 0 ] );
	exit( 1 );
    }

    tran->t_pinfo.pi_type = argv[ 0 ][ 0 ];

    epath = decode( argv[ 1 ] );
    if ( strcmp( epath, tran->t_pinfo.pi_name ) < 0 ) {
	printf( "%s: line %d: bad sort order\n",
		tran->t_name, tran->t_linenum );
	exit ( 1 );
    }
    strcpy( tran->t_pinfo.pi_name, epath );

    /* reading and parsing the line */
    switch( *argv[ 0 ] ) {
    case 'd':				    /* dir */
    case 'p':
    case 'D':
    case 's':
	if ( ac != 5 ) {
	    fprintf( stderr, "%s: line %d: expected 5 arguments, got %d\n",
		    tran->t_name, tran->t_linenum, ac );
	    exit( 1 );
	}
	tran->t_pinfo.pi_stat.st_mode = strtol( argv[ 2 ], NULL, 8 );
	tran->t_pinfo.pi_stat.st_uid = atoi( argv[ 3 ] );
	tran->t_pinfo.pi_stat.st_gid = atoi( argv[ 4 ] );
	break;

    case 'b':				    /* block or char */
    case 'c':
	if ( ac != 7 ) {
	    fprintf( stderr, "%s: line %d: expected 7 arguments, got %d\n",
		    tran->t_name, tran->t_linenum, ac );
	    exit( 1 );
	}
	tran->t_pinfo.pi_stat.st_mode = strtol( argv[ 2 ], NULL, 8 );
	tran->t_pinfo.pi_stat.st_uid = atoi( argv[ 3 ] );
	tran->t_pinfo.pi_stat.st_gid = atoi( argv[ 4 ] );
	tran->t_pinfo.pi_dev = makedev( ( unsigned )( atoi( argv[ 5 ] )), 
		( unsigned )( atoi( argv[ 6 ] )));
	break;

    case 'l':				    /* link */
    case 'h':				    /* hard */
	if ( ac != 3 ) {
	    fprintf( stderr, "%s: line %d: expected 3 arguments, got %d\n",
		    tran->t_name, tran->t_linenum, ac );
	    exit( 1 );
	}
	epath = decode( argv[ 2 ] );
	strcpy( tran->t_pinfo.pi_link, epath );
	break;

    case 'f':				    /* file */
	if ( ac != 8 ) {
	    fprintf( stderr, "%s: line %d: expected 8 arguments, got %d\n",
		    tran->t_name, tran->t_linenum, ac );
	    exit( 1 );
	}
	tran->t_pinfo.pi_stat.st_mode = strtol( argv[ 2 ], NULL, 8 );
	tran->t_pinfo.pi_stat.st_uid = atoi( argv[ 3 ] );
	tran->t_pinfo.pi_stat.st_gid = atoi( argv[ 4 ] );
	tran->t_pinfo.pi_stat.st_mtime = atoi( argv[ 5 ] );
	tran->t_pinfo.pi_stat.st_size = atoi( argv[ 6 ] );
	if ( tran->t_type != T_NEGATIVE ) {
	    if (( chksum ) && ( strcmp( "-", argv [ 7 ] ) == 0  )) {
		fprintf( stderr, "%s: line %d: no chksums in transcript\n",
			tran->t_name, tran->t_linenum );
		exit( 1 );
	    }
	}
	strcpy( tran->t_pinfo.pi_chksum_b64, argv[ 7 ] );
	break;

    case '-':
    case '+':
	fprintf( stderr, "%s: line %d: leading '%c' not allowed\n",
		tran->t_name, tran->t_linenum, *argv[ 0 ] );
	exit( 1 );		

    default:
	fprintf( stderr,
	    "%s: line %d: unknown file type '%c'\n",
	    tran->t_name, tran->t_linenum, *argv[ 0 ] );
	exit( 1 );
    }

    return;
}

    static char
t_convert( int type ) 
{
    switch( type ) {
    case S_IFREG:
	return ( 'f' );
    case S_IFDIR:
	return ( 'd' );
    case S_IFLNK:
	return ( 'l' );
    case S_IFCHR:
	return ( 'c' );
    case S_IFBLK:
	return ( 'b' );
#ifdef SOLARIS
    case S_IFDOOR:
	return ( 'D' );
#endif
    case S_IFIFO:
	return ( 'p' );
    case S_IFSOCK:
	return ( 's' );
    default:
	return ( 0 );
    }
}


    static void
t_print( struct pathinfo *fs, struct transcript *tran, int flag ) 
{
    struct pathinfo	*cur;
    char		*epath;
    dev_t		dev;
    extern int		edit_path;

    /*
     * Compare the current transcript with the previous one.  If they are 
     * different, print the name of the new transcript to the output file.
     * If the previous transcript has not been set, print out the current
     * transcript and set prev_tran 
     */
    if ( edit_path == FS2TRAN ) {
	cur = &tran->t_pinfo;
	if ( prev_tran != tran ) {
	    fprintf( outtran, "%s:\n", tran->t_name );
	    prev_tran = tran;
	}
    } else {
	cur = fs;
    }

    /*
     * If a file is missing from the edit_path that was chosen, a - is 
     * printed and then the file name that is missing is printed.
     */
    if (( edit_path == FS2TRAN ) && ( flag == PR_FS_ONLY )) {
	fprintf( outtran, "- " );
	cur = fs;
    } else if (( edit_path ==  TRAN2FS ) && ( flag == PR_TRAN_ONLY )) {
	fprintf( outtran, "- " );
	cur = &tran->t_pinfo;
    }

    epath = encode( cur->pi_name );

    /* print out info to file based on type */
    switch( cur->pi_type ) {
    case 'd':
    case 's':
    case 'D':
    case 'p':
	fprintf( outtran, "%c %-37s\t%.4lo %5d %5d\n", cur->pi_type, epath, 
		(unsigned long )( T_MODE & cur->pi_stat.st_mode ), 
		(int)cur->pi_stat.st_uid, (int)cur->pi_stat.st_gid );
	break;

    case 'l':
    case 'h':
	fprintf( outtran, "%c %-37s\t", cur->pi_type, epath );
	epath = encode( cur->pi_link );
	fprintf( outtran, "%s\n", epath );
	break;

    case 'f':
	if (( edit_path == FS2TRAN ) && (( flag == PR_TRAN_ONLY ) || 
		( flag == PR_DOWNLOAD ))) {
	    fprintf( outtran, "+ " );
	}
	fprintf( outtran, "f %-37s\t%.4lo %5d %5d %9d %7d %s\n", epath,
		(unsigned long)( T_MODE & cur->pi_stat.st_mode ), 
		(int)cur->pi_stat.st_uid, (int)cur->pi_stat.st_gid,
		(int)cur->pi_stat.st_mtime, (int)cur->pi_stat.st_size,
		cur->pi_chksum_b64 );
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
	exit( 1 );
    } 
}

     
   static int 
t_compare( struct pathinfo *cur, struct transcript *tran )
{
    int			ret = -1;
    mode_t		mode;
    mode_t		tran_mode;
    dev_t		dev;

    if ( tran->t_eof ) {
	ret = -1;
    } else {
	ret = strcmp( cur->pi_name, tran->t_pinfo.pi_name );
    }

    if ( ret > 0 ) {
	/* name is in the tran, but not the fs */
	t_print( cur, tran, PR_TRAN_ONLY ); 
	return T_MOVE_TRAN;
    } 
    if ( ret < 0 ) {
	/* name is in the fs, but not in the tran */
	if ( cur->pi_type == 'f' ) {
	    do_chksum( cur );
	}
	t_print( cur, tran, PR_FS_ONLY );
	return T_MOVE_FS;
    } 

    /* convert the modes */
    mode = ( T_MODE & cur->pi_stat.st_mode );
    tran_mode = ( T_MODE & tran->t_pinfo.pi_stat.st_mode );

    /* the names match so check types */
    if ( cur->pi_type != tran->t_pinfo.pi_type ) {
	t_print( cur, tran, PR_DOWNLOAD );
	return T_MOVE_BOTH;
    }

    /* compare the other components for each file type */
    switch( cur->pi_type ) {
    case 'f':			    /* file */
	if ( tran->t_type != T_NEGATIVE ) {
	    do_chksum( cur );
	    if (( chksum ) && 
	strcmp( cur->pi_chksum_b64, tran->t_pinfo.pi_chksum_b64 ) != 0 ) {
		t_print( cur, tran, PR_DOWNLOAD );
		break;
	    }

	    if (( cur->pi_stat.st_mtime != tran->t_pinfo.pi_stat.st_mtime ) ||
		    ( cur->pi_stat.st_size != tran->t_pinfo.pi_stat.st_size )) {
		t_print( cur, tran, PR_DOWNLOAD );
		break;
	    }
	}

	if (( cur->pi_stat.st_uid != tran->t_pinfo.pi_stat.st_uid ) || 
		( cur->pi_stat.st_gid != tran->t_pinfo.pi_stat.st_gid ) ||
		( mode != tran_mode )) {
	    t_print( cur, tran, PR_STATUS );
	}
	break;

    case 'd':				/* dir */
    case 'D':
    case 'p':
    case 's':
	if (( cur->pi_stat.st_uid != tran->t_pinfo.pi_stat.st_uid ) ||
		( cur->pi_stat.st_gid != tran->t_pinfo.pi_stat.st_gid ) ||
		( mode != tran_mode )) {
	    t_print( cur, tran, PR_STATUS );
	}
	break;

    case 'l':			    /* link */
    case 'h':			    /* hard */
	if ( strcmp( cur->pi_link, tran->t_pinfo.pi_link ) != 0 ) {
	    t_print( cur, tran, PR_STATUS );
	} 
	break;

    case 'c':
    case 'b':
	dev = cur->pi_stat.st_rdev;
	if (( cur->pi_stat.st_uid != tran->t_pinfo.pi_stat.st_uid ) ||
		( cur->pi_stat.st_gid != tran->t_pinfo.pi_stat.st_gid ) || 
		( dev != tran->t_pinfo.pi_dev ) ||
		( mode != tran_mode )) {
	    t_print( cur, tran, PR_STATUS );
	}	
	break;

    default:
	fprintf( stderr, "OOPS! XXX DOUBLE PANIC!\n" );
	break;
    }

    return T_MOVE_BOTH;
}

    int
transcript( struct pathinfo *new, char *name )
{

    int			move;
    int 		len;
    char		epath[ MAXPATHLEN ];
    char		*path;
    int			ret;
    int			type;
    struct transcript	*next_tran = NULL;
    struct transcript	*begin_tran = NULL;

    if ( lstat( name, &new->pi_stat ) != 0 ) {
	perror( name );
	exit( 1 );
    }

    type = ( S_IFMT & new->pi_stat.st_mode );
    if (( new->pi_type = t_convert( type )) == 0 ) {
	fprintf( stderr, "%s is of an uknown type\n", new->pi_name );
	exit ( 1 );
    }

    /* if it's multiply referenced, check if it's a hardlink */
    if ( !S_ISDIR( new->pi_stat.st_mode ) &&
	    ( new->pi_stat.st_nlink > 1 ) &&
	    (( path = hardlink( new )) != NULL )) {
	new->pi_type = 'h';
	strcpy( new->pi_link, path );
    }

    /* check to see if a link, then read it in */
    if ( S_ISLNK( new->pi_stat.st_mode )) {
	len = readlink( new->pi_name, epath, MAXPATHLEN );
	epath[ len ] = '\0';
	strcpy( new->pi_link, epath );
    }

    /* only go into the file if it is a directory */
    if ( S_ISDIR( new->pi_stat.st_mode )) {
	move = 1;
    } else { 
	move = 0;
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
		if ( strcmp( next_tran->t_pinfo.pi_name,
			begin_tran->t_pinfo.pi_name ) < 0 ) {
		    begin_tran = next_tran;
		}
	    }
	}

	/* move ahead other transcripts that match */
	for ( next_tran = begin_tran->t_next; next_tran != NULL;
		next_tran = next_tran->t_next ) {
	    if ( strcmp( begin_tran->t_pinfo.pi_name,
		    next_tran->t_pinfo.pi_name ) == 0 ) {
		t_parse( next_tran );
	    }
	}

	/*
	 * t_compare returns similar values as strcmp depending on
	 * what the return value of the internal strcmp is.
	 */
	ret = t_compare( new, begin_tran );

	switch ( ret ) {
	case T_MOVE_FS :	   /*
				    * either there is no transcript, or the
				    * fs value is alphabetically before the
				    * transcript value. move the fs forward. 
				    */
	    return( move );

	case T_MOVE_BOTH :	  /* the two values match. move ahead in both */
	    t_parse( begin_tran );
	    if ( begin_tran->t_type == T_NEGATIVE ) {
		return( 0 );
	    } else {
		return( move );
	    }

	case T_MOVE_TRAN :	   /*
				    * the fs value is alphabetically after the
				    * transcript value.  move the transcript
				    * forward 
				    */
	    t_parse( begin_tran );
	    break;

	default :
	    fprintf( stderr, "OOPS! XXX FAMINE and DESPAIR!\n" );
	    exit( 1 );
	}
    }
}

    static void
t_new( int type, char *name ) 
{
    struct transcript	 *new;

    if (( new = (struct transcript *)malloc( sizeof( struct transcript )))
	    == NULL ) {
	perror( "malloc" );
	exit( 1 );
    }

    new->t_type = type;
    if ( new->t_type == T_NULL ) {
	new->t_eof = 1; 

    } else {
	new->t_eof = 0; 
	new->t_linenum = 0;
	strcpy( new->t_name, name );
	if (( new->t_in = fopen( name, "r" )) == NULL ) {
	    perror( name );
	    exit( 1 );
	}

	t_parse( new );
    }

    new->t_next = tran_head;
    tran_head = new;

    return;
}

    void
transcript_init(  char *cmd )
{
    char	**av;
    char	line[ MAXPATHLEN ];
    int		length, ac;
    int		special = 0;
    extern int	edit_path;
    FILE	*fp;

    if ( skip ) {
	t_new( T_NULL, NULL );
	return;
    }

    if (( fp = fopen( cmd, "r" )) == NULL ) {
	if ( edit_path == FS2TRAN ) {
	    perror( cmd );
	    exit( 1 );
	}
	t_new( T_NULL, NULL );
	return;
    }

    while ( fgets( line, sizeof( line ), fp ) != NULL ) {
	linenum++;
	length = strlen( line );
	if ( line[ length - 1 ] != '\n' ) {
	    fprintf( stderr, "command: line %d: line too long\n", linenum );
	    exit( 1 );
	}

	/* skips blank lines and comments */
	if ((( ac = argcargv( line, &av )) == 0 ) || ( *av[ 0 ] == '#' )) {
	    continue;
	}

	if ( ac != 2 ) {
	    fprintf( stderr, "command: line %d: expected 2 arguments, got %d\n",
		    linenum, ac );
	    exit ( 1 );
	} 

	if ( strlen( av[ 1 ] ) > MAXPATHLEN ) {
	    fprintf( stderr, "command: line %d: transcript name too long\n",
		    linenum );
	    fprintf( stderr, "command: line %d: %s\n", linenum, av[ 1 ] );
	    exit( 1 );
	}

	switch( *av[ 0 ] ) {
	case 'p':				/* positive */
	    t_new( T_POSITIVE, av[ 1 ] );
	    break;
	case 'n':				/* negative */
	    t_new( T_NEGATIVE, av[ 1 ] );
	    break;
	case 's':				/* special */
	    special = 1;
	    continue;
	default:
	    fprintf( stderr, "command: line %d: '%s' invalid\n",
		    linenum, av[ 0 ] );
	    exit( 1 );
	}
    }

    fclose( fp );

    /* open the special transcript if there were any special files */
    if ( special == 1 ) {
	t_new( T_SPECIAL, "special.T" );
    }

    return;
}

    void
transcript_free( )
{
    struct transcript	 *next;

    while ( tran_head != NULL ) {
	next = tran_head->t_next;
	if ( tran_head->t_in != NULL ) {
	    fclose( tran_head->t_in );
	}
	free( tran_head );
	tran_head = next;
    }
}
