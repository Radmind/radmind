#include <stdio.h>
#include <stdlib.h>
#include <sys/mkdev.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <unistd.h>

#include "transcript.h"
#include "llist.h"
#include "code.h"
#include "argcargv.h"

static struct transcript *tran_head;
static FILE 		 *com;
static struct transcript *prev_tran;

    static void 
t_parse( struct transcript *tran ) 
{
    char	        	line[ 2 * MAXPATHLEN ];
    int				length;
    char			*buf;
    char			**argv;
    int				ac;

    /* read in the next line in the transcript */
    if (( fgets( line, MAXPATHLEN, tran->t_in )) == NULL ) {
	tran->t_flag = T_EOF;
	return;
    }

    /* check to see if line contains the whole line */
    length = strlen( line );
    if ( line[ length - 1 ] != '\n' ) {
	fprintf( stderr, "ERROR: didn't get the whole line\n" );
	exit( 1 );
    } 
    line[ length - 1 ] = '\0';

    /* parse the line that was read in */
    ac = argcargv( line, &argv );

    /* 
     * If the line begins with either a - or a string that is more than one
     * char long, print an error.
     */
    if ( strcmp( argv[0], "-" ) == 0 ) {
	fprintf( stderr, "ERROR: Incorrect type of transcript\n" );
	exit( 1 );
    }

    if ( strlen( argv[ 0 ] ) != 1 ) {
	fprintf( stderr, "ERROR: Incorrect form of transcript\n" );
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
	tran->t_info.i_maj = atoi( argv[ 4 ] );
	tran->t_info.i_min = atoi( argv[ 5 ] );
	buf = decode( argv[ 6 ] );
	strcpy( tran->t_info.i_name, buf );
	break;

    case 'l':				    /* link */
	if ( ac != 4 ) {
	    fprintf( stderr, "Incorrect number of arguments in transcript\n" );
	    exit( 1 );
	}
	tran->t_info.i_stat.st_mode = strtol( argv[ 1 ], NULL, 8 );
	buf = decode( argv[ 2 ] );
	strcpy( tran->t_info.i_link, buf );
	buf = decode( argv[ 3 ] );
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

    default:
	fprintf( stderr, "ERROR: Unknown file type!\n" );
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
    case S_IFDOOR:
	return ( 'D' );
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
t_print( struct info *fs, struct transcript *tran, FILE *outtran, int change ) 
{
    struct info	*cur;
    char 	*buf;
    char	*link;
    major_t	maj;
    minor_t	min;
    extern int  edit_path;

    /*
     * Compare the current transcript with the previous one.  If they are 
     * different, print the name of the new transcript to the output file.
     * If the previous transcript has not been set, print out the current
     * transcript and set prev_tran 
     */
    if ( edit_path == FS2TRAN ) {
	cur = &tran->t_info;
    	if ( prev_tran != NULL ) {
    	  if ( strcmp( prev_tran->t_name, tran->t_name ) != 0 ) {
		fprintf( outtran, "%s:\n", tran->t_name );
    	  }
        } else {
	  fprintf( outtran, "%s:\n", tran->t_name );
        }
	prev_tran = tran;
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
		( T_MODE & cur->i_stat.st_mode ), 
		(int)cur->i_stat.st_uid, (int)cur->i_stat.st_gid, buf );
	break;
    case 'l':
	link = encode( cur->i_link );
	fprintf( outtran, "l %4lo %s", ( T_MODE & cur->i_stat.st_mode ), link );
	buf = encode( cur->i_name );
	fprintf( outtran, " %s\n", buf );
	break;
    case 'f':
	fprintf( outtran, "f %4lo %5d %5d %9d %7d %3d %s\n", 
		( T_MODE & cur->i_stat.st_mode ), (int)cur->i_stat.st_uid,
		(int)cur->i_stat.st_gid, (int)cur->i_stat.st_mtime,
		(int)cur->i_stat.st_size, cur->i_chksum, buf );
	break;
    default:
    case 'c':
    case 'b':
    case 's':
    case 'D':
    case 'p':
	maj = major( cur->i_stat.st_rdev );
	min = minor( cur->i_stat.st_rdev );
	fprintf( outtran, "%c %4lo %4d %5d %5d %5d %s\n", 
		cur->i_type, ( T_MODE & cur->i_stat.st_mode ), 
		(int)cur->i_stat.st_uid,
		(int)cur->i_stat.st_gid, (int)maj, (int)min, buf );
	break;
    } 
}

     
   static int 
t_compare( struct info *cur, struct transcript *tran, FILE *outtran )
{
    int		    		ret = -1;
    mode_t    	    		mode;
    mode_t	    		tran_mode;
    major_t         		maj;
    minor_t         		min;

    /* writing */
    if ( tran->t_flag == T_EOF ) {
	ret = -1;
    } else {
	ret = strcmp( cur->i_name, tran->t_info.i_name );
    }

    if ( ret > 0 ) {
	/* name is in the tran, but not the fs */
	t_print( cur, tran, outtran, T_MOVE_TRAN );
	return T_MOVE_TRAN;
    } 
    if ( ret < 0 ) {
	/* name is in the fs, but not in the tran */
	t_print( cur, tran, outtran, T_MOVE_FS );
	return T_MOVE_FS;
    } 

    /* convert the modes */
    mode = ( T_MODE & cur->i_stat.st_mode );
    tran_mode = ( T_MODE & tran->t_info.i_stat.st_mode );

    /* the names match so check types */
    if ( cur->i_type != tran->t_info.i_type ) {
	t_print( cur, tran, outtran, T_MOVE_BOTH );
	return T_MOVE_BOTH;
    }

    /* compare the other components for each file type */
    switch( cur->i_type ) {
    case 'f':			    /* file */
        if (( cur->i_stat.st_uid != tran->t_info.i_stat.st_uid ) || 
    		( cur->i_stat.st_gid != tran->t_info.i_stat.st_gid ) ||
		( mode != tran_mode )) {
			t_print( cur, tran, outtran, T_MOVE_BOTH );
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
			t_print( cur, tran, outtran, T_MOVE_BOTH );
		}
    	}
	break;

    case 'd':				/* dir */
        if (( cur->i_stat.st_uid != tran->t_info.i_stat.st_uid ) ||
	    	( cur->i_stat.st_gid != tran->t_info.i_stat.st_gid ) ||
		( mode != tran_mode )) {
			t_print( cur, tran, outtran, T_MOVE_BOTH );
        }
        break;

    case 'l':			    /* link */
        if ((( strcmp( cur->i_link, tran->t_info.i_link )) != 0 )  ||
		( mode != tran_mode )) {
			t_print( cur, tran, outtran, T_MOVE_BOTH );
	} 
	break;
    case 'c':
    case 'b':
    case 'D':
    case 'p':
    case 's':
	maj = major( cur->i_stat.st_rdev );
	min = minor( cur->i_stat.st_rdev );
	if (( cur->i_stat.st_uid != tran->t_info.i_stat.st_uid ) ||
	    	( cur->i_stat.st_gid != tran->t_info.i_stat.st_gid ) || 
	    	( maj != tran->t_info.i_maj ) || 
	    	( min != tran->t_info.i_min ) ||
		( mode != tran_mode )) {
			t_print( cur, tran, outtran, T_MOVE_BOTH );
	}	
	break;
    default:
	fprintf( stderr, "ERROR: Unknown file type\n" );
	break;
    }

    return T_MOVE_BOTH;
}

    int
transcript( struct info *new, char *name, FILE *outtran )
{

    int			move;
    int 		count;
    char		buf[ MAXPATHLEN ];
    int			ret;
    int			comp;
    int			type;
    struct transcript	*t_cur = NULL;
    struct transcript	*begin_tran = NULL;

    if ( lstat( name, &new->i_stat ) != 0 ) {
	perror( name );
	exit( 1 );
    }

    type = ( S_IFMT & new->i_stat.st_mode );
    new->i_type = t_convert( type );

    /* check to see if a link, then read it in */
    if ( S_ISLNK( new->i_stat.st_mode )) {
	count = readlink( new->i_name, buf, MAXPATHLEN );
	buf[ count ] = '\0';
	strcpy( new->i_link, buf );
    }

    /* only go into the file if it is a directory */
    if ( S_ISDIR( new->i_stat.st_mode )) {
	move = 1;
    } else { 
	move = 0;
    }

    for ( ; ; ) {
	/* 
	 * Loop through the list of transcripts and compare each
	 * to find which transcript to start with. Only switch to the
	 * transcript if it is not at EOF.
	 */
        for ( begin_tran = tran_head, t_cur = tran_head->t_next;
		    t_cur != NULL; t_cur = t_cur->t_next ) {
		if ( begin_tran->t_flag == T_EOF ) {
		    begin_tran = t_cur;
		    continue;
		}
		if ( t_cur->t_flag != T_EOF ) {
		    /* call strcmp on the two transcripts */
		    comp = strcmp( begin_tran->t_info.i_name, 
			    t_cur->t_info.i_name );
		    if ( comp > 0 ) {
			begin_tran = t_cur;
		    }
		}
        }

     	/* move ahead other transcripts that match */
        for( t_cur = begin_tran->t_next; t_cur != NULL; 
			t_cur = t_cur->t_next ) {
	     if ( strcmp( begin_tran->t_info.i_name, t_cur->t_info.i_name )
					== 0 ) {
			t_parse( t_cur );
	     }
     	}

	/*
	 * t_compare returns similar values as strcmp depending on
	 * what the return value of the internal strcmp is.
	 */
	ret = t_compare( new, begin_tran, outtran );

	switch ( ret ) {
	case T_MOVE_FS :	   /*
				    * either there is no transcript, or the
				    * fs value is alphabetically before the
				    * transcript value. move the fs forward. 
				    */
	    return( move );

	case T_MOVE_BOTH :	  /* the two values match. move ahead in both */
	    /* t_compare() can't return 0 if begin_tran is NULL */
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
	    /* t_compare() can't return -1 if begin_tran is NULL */
	    t_parse( begin_tran );
	    break;

	default :
	    fprintf( stderr, "Oops!\n" );
	    exit( 1 );
	}
     }
}

    static struct transcript *
t_new( char *name, struct transcript *head ) 
{
    struct transcript    *new;

    if (( new = (struct transcript *)malloc( sizeof( struct transcript )))
	    == NULL ) {
	perror( "malloc" );
	exit( 1 );
    }

    strcpy( new->t_info.i_name, name );
    new->t_next = head;
    new->t_flag = T_EOF; 

    return( new );
}

    void
transcript_init( )
{
    char	**av;
    char	line[ MAXPATHLEN ];
    int		length;
    int		special = 0;
    extern int	edit_path;

    tran_head = NULL;

    /*
     * open the command file.  read in each line of the file and determine 
     *  which type of transcript it is.
     */
    if (( com = fopen( "command", "r" )) != NULL ) {
	while ( fgets( line, sizeof( line ), com ) != NULL ) {
    		length = strlen( line );
    		if ( line[ length - 1 ] != '\n' ) {
			fprintf( stderr, "ERROR: didn't get the whole line\n" );
			exit( 1 );
    		} 
		argcargv( line, &av );
		tran_head = t_new( av[ 1 ], tran_head );
		switch( *av[0] ) {
		case 'p':				/* positive */
			tran_head->t_type = T_POSITIVE;
			break;
		case 'n':				/* negative */
			tran_head->t_type = T_NEGATIVE;
			break;
		case 's':				/* special */
			special = 1;
			continue;
		default:
			fprintf( stderr, "Invalid type of transcript\n" );
			exit( 1 );
		}

		/* open transcript and parse the first line */
		if (( tran_head->t_in = fopen( av[ 1 ], "r" )) == NULL ) {
			perror( av[ 1 ] );
			exit( 1 );
    		}
		if ( strlen( av[ 1 ] ) > MAXPATHLEN ) {
			fprintf( stderr, "ERROR: argument is too large\n" );
			exit( 1 );
		}
		strcpy( tran_head->t_name, av[ 1 ] );
		tran_head->t_flag = 1;

		t_parse( tran_head );
        }
     } else {
	if ( edit_path == FS2TRAN ) {
		fprintf( stderr, "ERROR: Target cannot be NULL transcript.\n" );
		exit( 1 );
	}
	tran_head = t_new( "NULL", tran_head );
     }


     /* 
      * open the special transcript if there are any specials and add a
      * specail transcript node to the beginning of the transcript list  
      */
     if ( special == 1 ) {
	if (( tran_head->t_in = fopen( "special.T", "r" )) == NULL ) {
		perror( "special" );
		exit( 1 );
 	}
	tran_head = t_new( "special.T", tran_head );
	tran_head->t_type = T_SPECIAL;
     }

     return;
}


    void
transcript_free( )
{
    struct transcript    *next;

    for ( ; tran_head != NULL; tran_head = next ) {
	next = tran_head->t_next;
	fclose( tran_head->t_in );
	free( tran_head );
    }
    free( com );
}
