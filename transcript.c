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

static struct transcript *tran_head = NULL;
static struct transcript *prev_tran = NULL;

    static void 
t_parse( struct transcript *tran ) 
{
    char	        	line[ 2 * MAXPATHLEN ];
    int				length;
    char			*buf;
    char			**argv;
    int				ac;

    /* read in the next line in the transcript, loop through blanks and # */
    do {
	if (( fgets( line, MAXPATHLEN, tran->t_in )) == NULL ) {
	    tran->t_eof = 1;
	    return;
	}

	/* check to see if line contains the whole line */
	length = strlen( line );
	if ( line[ length - 1 ] != '\n' ) {
	    fprintf( stderr, "ERROR: didn't get the whole line\n" );
	    exit( 1 );
	} 
	line[ length - 1 ] = '\0';

    } while ((( ac = argcargv( line, &argv )) == 0 ) || ( *argv[ 0 ] == '#' ));

    if ( strlen( argv[ 0 ] ) != 1 ) {
	fprintf( stderr, "ERROR: First argument in transcript is too long: %s\n"		, argv[ 0 ] );
	exit( 1 );
    }

    tran->t_info.i_type = argv[ 0 ][ 0 ];

    /* reading and parsing the line */
    switch( *argv[ 0 ] ) {
    case 'd':				    /* dir */
	if ( ac != 5 ) {
	    fprintf( stderr, "Incorrect number of arguments in transcript\n" );
	    exit( 1 );
	}
	tran->t_info.i_stat.st_mode = strtol( argv[ 1 ], NULL, 8 );
	tran->t_info.i_stat.st_uid = atoi( argv[ 2 ] );
	tran->t_info.i_stat.st_gid = atoi( argv[ 3 ] );
	buf = decode( argv[ 4 ] );
	strcpy( tran->t_info.i_name, buf );
	break;

    case 'b':				    /* block or char */
    case 'c':
    case 'p':
    case 'D':
    case 's':
	if ( ac != 7 ) {
	    fprintf( stderr, "Incorrect number of arguments in transcript\n" );
	    exit( 1 );
	}
	tran->t_info.i_stat.st_mode = strtol( argv[ 1 ], NULL, 8 );
	tran->t_info.i_stat.st_uid = atoi( argv[ 2 ] );
	tran->t_info.i_stat.st_gid = atoi( argv[ 3 ] );
#ifdef notdef
	tran->t_info.i_maj = atoi( argv[ 4 ] );
	tran->t_info.i_min = atoi( argv[ 5 ] );
#endif
	tran->t_info.i_dev = makedev( ( unsigned )( atoi( argv[ 4 ] )), 
		( unsigned )( atoi( argv[ 5 ] )));
	buf = decode( argv[ 6 ] );
	strcpy( tran->t_info.i_name, buf );
	break;

    case 'l':				    /* link */
    case 'h':				    /* hard */
	if ( ac != 3 ) {
	    fprintf( stderr, "Incorrect number of arguments in transcript\n" );
	    exit( 1 );
	}
	buf = decode( argv[ 1 ] );
	strcpy( tran->t_info.i_link, buf );
	buf = decode( argv[ 2 ] );
	strcpy( tran->t_info.i_name, buf );
	break;

    case 'f':				    /* file */
	if ( ac != 8 ) {
	    fprintf( stderr, "Incorrect number of arguments in transcript\n" );
	    exit( 1 );
	}
	tran->t_info.i_stat.st_mode = strtol( argv[ 1 ], NULL, 8 );
	tran->t_info.i_stat.st_uid = atoi( argv[ 2 ] );
	tran->t_info.i_stat.st_gid = atoi( argv[ 3 ] );
	tran->t_info.i_stat.st_mtime = atoi( argv[ 4 ] );
	tran->t_info.i_stat.st_size = atoi( argv[ 5 ] );
	tran->t_info.i_chksum = atoi( argv[ 6 ] );
	buf = decode( argv[ 7 ] );
	strcpy( tran->t_info.i_name, buf );
	break;

    case '-':
	fprintf( stderr, "ERROR: Leading -'s are not allowed: %s\n", line );
	exit( 1 );              

    default:
	fprintf( stderr, "ERROR: Unknown file type: %c\n", *argv[ 0 ] );
	break;
    }

    tran->t_info.i_chksum = 0;
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
	fprintf( stderr, "ERROR: Unknown file type %c\n", type );
	return ( '0' );
    }
}


    static void
t_print( struct info *fs, struct transcript *tran, int change ) 
{
    struct info	*cur;
    char 	*buf;
    char	*link;
#ifdef notdef
    major_t	maj;
    minor_t	min;
#endif
    dev_t	dev;
    extern int  edit_path;

    /*
     * Compare the current transcript with the previous one.  If they are 
     * different, print the name of the new transcript to the output file.
     * If the previous transcript has not been set, print out the current
     * transcript and set prev_tran 
     */
    if ( edit_path == FS2TRAN ) {
	cur = &tran->t_info;

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
    if (( edit_path == FS2TRAN ) && ( change == T_MOVE_FS )) {
	fprintf( outtran, "- " );
	cur = fs;
    } else if (( edit_path ==  TRAN2FS ) && ( change == T_MOVE_TRAN )) {
	fprintf( outtran, "- " );
	cur = &tran->t_info;
    } else {
	fprintf( outtran, "  " );
    }

    buf = encode( cur->i_name );

    /* print out info to file based on type */
    switch( cur->i_type ) {
    case 'd':
	fprintf( outtran, "d %4lo %5d %5d %s\n", 
		(unsigned long )( T_MODE & cur->i_stat.st_mode ), 
		(int)cur->i_stat.st_uid, (int)cur->i_stat.st_gid, buf );
	break;
    case 'l':
    case 'h':
	link = encode( cur->i_link );
	fprintf( outtran, "%c %s", cur->i_type, link );
	buf = encode( cur->i_name );
	fprintf( outtran, " %s\n", buf );
	break;
    case 'f':
	fprintf( outtran, "f %4lo %5d %5d %9d %7d %3d %s\n", 
		(unsigned long)( T_MODE & cur->i_stat.st_mode ), 
		(int)cur->i_stat.st_uid,
		(int)cur->i_stat.st_gid, (int)cur->i_stat.st_mtime,
		(int)cur->i_stat.st_size, cur->i_chksum, buf );
	break;
    default:
    case 'c':
    case 'b':
    case 's':
    case 'D':
    case 'p':
	dev = cur->i_stat.st_rdev;
#ifdef notdef
	maj = major( cur->i_stat.st_rdev );
	min = minor( cur->i_stat.st_rdev );
#endif
	fprintf( outtran, "%c %4lo %4d %5d %5d %5d %s\n", 
		cur->i_type, 
		(unsigned long )( T_MODE & cur->i_stat.st_mode ), 
		(int)cur->i_stat.st_uid,
		(int)cur->i_stat.st_gid, (int)major(dev), (int)minor(dev), 
		buf );
	break;
    } 
}

     
   static int 
t_compare( struct info *cur, struct transcript *tran )
{
    int		    		ret = -1;
    mode_t    	    		mode;
    mode_t	    		tran_mode;
#ifdef notdef
    major_t         		maj;
    minor_t         		min;
#endif
    dev_t			dev;

    /* writing XXX what does this comment mean? */
    if ( tran->t_eof ) {
	ret = -1;
    } else {
	ret = strcmp( cur->i_name, tran->t_info.i_name );
    }

    if ( ret > 0 ) {
	/* name is in the tran, but not the fs */
	t_print( cur, tran, T_MOVE_TRAN );
	return T_MOVE_TRAN;
    } 
    if ( ret < 0 ) {
	/* name is in the fs, but not in the tran */
	t_print( cur, tran, T_MOVE_FS );
	return T_MOVE_FS;
    } 

    /* convert the modes */
    mode = ( T_MODE & cur->i_stat.st_mode );
    tran_mode = ( T_MODE & tran->t_info.i_stat.st_mode );

    /* the names match so check types */
    if ( cur->i_type != tran->t_info.i_type ) {
	t_print( cur, tran, T_MOVE_BOTH );
	return T_MOVE_BOTH;
    }

    /* compare the other components for each file type */
    switch( cur->i_type ) {
    case 'f':			    /* file */
        if (( cur->i_stat.st_uid != tran->t_info.i_stat.st_uid ) || 
    		( cur->i_stat.st_gid != tran->t_info.i_stat.st_gid ) ||
		( mode != tran_mode )) {
			t_print( cur, tran, T_MOVE_BOTH );
			break;
	}
	/* If the file is not negative, check the other components. */
	if ( tran->t_type != T_NEGATIVE ) {
    		if (( cur->i_stat.st_mtime != 
				tran->t_info.i_stat.st_mtime ) ||
    		    ( cur->i_stat.st_size != 
				tran->t_info.i_stat.st_size ) || 
    		    ( cur->i_chksum != tran->t_info.i_chksum ) ||
		    ( mode != tran_mode )) {
			t_print( cur, tran, T_MOVE_BOTH );
		}
	}
	break;

    case 'd':				/* dir */
        if (( cur->i_stat.st_uid != tran->t_info.i_stat.st_uid ) ||
	    	( cur->i_stat.st_gid != tran->t_info.i_stat.st_gid ) ||
		( mode != tran_mode )) {
			t_print( cur, tran, T_MOVE_BOTH );
        }
        break;

    case 'l':			    /* link */
    case 'h':			    /* hard */
        if ( strcmp( cur->i_link, tran->t_info.i_link ) != 0 ) {
	    t_print( cur, tran, T_MOVE_BOTH );
	} 
	break;
    case 'c':
    case 'b':
    case 'D':
    case 'p':
    case 's':
#ifdef notdef
	maj = major( cur->i_stat.st_rdev );
	min = minor( cur->i_stat.st_rdev );
#endif
	dev = cur->i_stat.st_rdev;
	if (( cur->i_stat.st_uid != tran->t_info.i_stat.st_uid ) ||
	    	( cur->i_stat.st_gid != tran->t_info.i_stat.st_gid ) || 
		( dev != tran->t_info.i_dev ) ||
#ifdef notdef
	    	( maj != tran->t_info.i_maj ) || 
	    	( min != tran->t_info.i_min ) ||
#endif
		( mode != tran_mode )) {
			t_print( cur, tran, T_MOVE_BOTH );
	}	
	break;

    default:
	fprintf( stderr, "ERROR: Unknown file type: %c\n", cur->i_type );
	break;
    }

    return T_MOVE_BOTH;
}

    int
transcript( struct info *new, char *name )
{

    int			move;
    int 		len;
    char		buf[ MAXPATHLEN ];
    char		*path;
    int			ret;
    int			type;
    struct transcript	*next_tran = NULL;
    struct transcript	*begin_tran = NULL;

    if ( lstat( name, &new->i_stat ) != 0 ) {
	perror( name );
	return( 0 );
    }

    type = ( S_IFMT & new->i_stat.st_mode );
    new->i_type = t_convert( type );

    /* if it's multiply referenced, check if it's a hardlink */
    if ( !S_ISDIR( new->i_stat.st_mode ) &&
	    ( new->i_stat.st_nlink > 1 ) &&
	    (( path = hardlink( new )) != NULL )) {
	new->i_type = 'h';
	strcpy( new->i_link, path );
    }

    /* check to see if a link, then read it in */
    if ( S_ISLNK( new->i_stat.st_mode )) {
	len = readlink( new->i_name, buf, MAXPATHLEN );
	buf[ len ] = '\0';
	strcpy( new->i_link, buf );
    }

    /* only go into the file if it is a directory */
    if ( S_ISDIR( new->i_stat.st_mode )) {
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
		if ( strcmp( next_tran->t_info.i_name,
			begin_tran->t_info.i_name ) < 0 ) {
		    begin_tran = next_tran;
		}
	    }
	}

	/* move ahead other transcripts that match */
	for ( next_tran = begin_tran->t_next; next_tran != NULL;
		next_tran = next_tran->t_next ) {
	    if ( strcmp( begin_tran->t_info.i_name,
		    next_tran->t_info.i_name ) == 0 ) {
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
	    fprintf( stderr, "Oops!\n" );
	    exit( 1 );
	}
    }
}

    static void
t_new( int type, char *name ) 
{
    struct transcript    *new;

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
transcript_init( int flag )
{
    char	**av;
    char	line[ MAXPATHLEN ];
    int		length;
    int		special = 0;
    extern int	edit_path;
    FILE 	*f;

    /*
     * open the command file.  read in each line of the file and determine 
     * which type of transcript it is.
     */
    if (( flag & FLAG_SKIP ) || (( f = fopen( "command", "r" )) == NULL )) {
	if ( edit_path == FS2TRAN ) {
	    fprintf( stderr, "ERROR: Target cannot be NULL transcript.\n" );
	    exit( 1 );
	}
	t_new( T_NULL, NULL );
	return;
     }

    while ( fgets( line, sizeof( line ), f ) != NULL ) {

	/* count lines for better error */

	length = strlen( line );
	if ( line[ length - 1 ] != '\n' ) {
	    fprintf( stderr, "ERROR: line too long\n" );
	    exit( 1 );
	}

	/* skips blank lines and comments */
	if (( argcargv( line, &av ) == 0 ) || ( *av[ 0 ] == '#' )) {
	    continue;
	}

	if ( strlen( av[ 1 ] ) > MAXPATHLEN ) {
	    fprintf( stderr, "%s: transcript name too long\n",
		    av[ 1 ] );
	    exit( 1 );
	}

	switch( *av[0] ) {
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
	    fprintf( stderr, "Invalid type of transcript\n" );
	    exit( 1 );
	}
    }

    fclose( f );

    /* open the special transcript if there were any special files */
    if ( special == 1 ) {
	t_new( T_SPECIAL, "special.T" );
    }

    return;
}

    void
transcript_free( )
{
    struct transcript    *next;

    while ( tran_head != NULL ) {
	next = tran_head->t_next;
	if ( tran_head->t_in != NULL ) {
	    fclose( tran_head->t_in );
	}
	free( tran_head );
	tran_head = next;
    }
}
