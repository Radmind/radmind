#include <stdio.h>
#include <sys/mkdev.h>
#include <sys/stat.h>

#include "transcript.h"
#include "llist.h"
#include "code.h"

static struct transcript *tran_head;
static FILE 		 *com;

    static void 
t_parse( struct transcript *tran ) 
{
    char	        	line[ MAXPATHLEN ];
    int				length;
    int				llength;
    char			temp[ MAXPATHLEN ]; 
    char			*buf;
    int				i = 0, b = 0;
    char			**argv;
    char			c;
    int				ac;

    if ( tran == NULL ) {
	return;
    }

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

    ac = argcargv( line, &argv );

    if ( strlen( argv[ 0 ] ) != 1 ) {
	fprintf( stderr, "ERROR: Incorrect form of transcript\n" );
	exit( 1 );
    }

    /* reading */
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
	tran->t_info.i_stat.st_ctime = atoi( argv[ 4 ] );
	tran->t_info.i_stat.st_size = atoi( argv[ 5 ] );
	tran->t_info.i_chksum = atoi( argv[ 6 ] );
	buf = decode( argv[ 7 ] );
	strcpy( tran->t_info.i_name, buf );
	break;

    default:
	fprintf( stderr, "ERROR: Incorrect file type\n" );
	break;
    }

    tran->t_info.i_chksum = 0;
    return;
}

    static void
t_printfs( struct info *cur, FILE *out, char *rpath )
{
    char 	*buf;
    char	*link;
    int		type;
    major_t	maj;
    minor_t	min;

    /* encode the name */
    buf = encode( cur->i_name );
    type = S_IFMT & cur->i_stat.st_mode;

    /* print out info to file based on type */
    switch( type ) {
    case S_IFDIR:
	fprintf( out, "d %6o %5d %5d", cur->i_stat.st_mode, 
		cur->i_stat.st_uid, cur->i_stat.st_gid );
	if ( strcmp( rpath, cur->i_name ) != 0 ) {
		fprintf( out, " /%s\n", buf );
	} else {
		fprintf( out, " %s\n", buf );
	}
	break;
    case S_IFLNK:
	link = encode( cur->i_link );
	fprintf( out, "l %6o %s", cur->i_stat.st_mode, link );
	buf = encode( cur->i_name );
	fprintf( out, " /%s\n", buf );
	break;
    case S_IFREG:
	fprintf( out, "f %6o %5d %5d %9d %7d %3d /%s\n", 
		cur->i_stat.st_mode, cur->i_stat.st_uid,
		cur->i_stat.st_gid, cur->i_stat.st_ctime,
		cur->i_stat.st_size, cur->i_chksum,
		buf );
	break;
    default:
	maj = major( cur->i_stat.st_rdev );
	min = minor( cur->i_stat.st_rdev );
	if ( S_ISBLK( cur->i_stat.st_mode )) {
		fprintf( out, "b " );
	} else if ( S_ISCHR( cur->i_stat.st_mode )) {
		fprintf( out, "c " );
	} else if ( S_ISDOOR( cur->i_stat.st_mode )) {
		fprintf( out, "D " );
	} else if ( S_ISFIFO( cur->i_stat.st_mode )) {
		fprintf( out, "p " );
	} else if ( S_ISSOCK( cur->i_stat.st_mode )) {
		fprintf( out, "s " );
	} else { 
		fprintf( stderr, "ERROR: Incorrect file type\n" );
		break;
	}
	fprintf( out, "%o %4d %5d %5d %5d /%s\n", 
		cur->i_stat.st_mode, cur->i_stat.st_uid,
		cur->i_stat.st_gid, maj, min, buf );
	break;
    } 
}

   static int 
t_compare( struct llist *cur, struct transcript *tran, char *rpath,
		FILE *out ) 
{
    int		    		ret = -1;
    char	    		name[ MAXPATHLEN ];
    mode_t    	    		mode;
    mode_t	    		type;
    mode_t	    		tran_mode;
    mode_t	    		tran_type;
    major_t         		maj;
    minor_t         		min;

    if ( strcmp( cur->ll_info.i_name, rpath ) == 0 ) {
	strcpy( name, cur->ll_info.i_name );
    } else {
        sprintf( name, "/%s", cur->ll_info.i_name );
    }

    /* writing */
    if ( tran == NULL ) {
	/* don't do the strcmp because there is nothing to check against. */
	ret = -1;
    } else if ( tran->t_flag != T_EOF ) {
	ret = strcmp( name, tran->t_info.i_name );
    }

    if ( ret > 0 ) {
	/* name is in the tran, but not the fs */
	t_printfs( &cur->ll_info, out, rpath );
	return 1;
    } 
    if ( ret < 0 ) {
	/* name is in the fs, but not in the tran */
	t_printfs( &cur->ll_info, out, rpath );
	return -1;
    } 

    /* the names match so check types */
    mode = ( S_IAMB & cur->ll_info.i_stat.st_mode );
    type = ( S_IFMT & cur->ll_info.i_stat.st_mode );
    tran_mode = ( S_IAMB & tran->t_info.i_stat.st_mode );
    tran_type = ( S_IFMT & tran->t_info.i_stat.st_mode );
    if ( tran_type != type ) {
        fprintf( stderr, "ERROR: Incorrect file type!\n" );
        return -1;
    }

    switch( tran_type ) {
    case S_IFREG:			    /* file */
        if (( cur->ll_info.i_stat.st_uid != tran->t_info.i_stat.st_uid ) || 
    		( cur->ll_info.i_stat.st_gid != tran->t_info.i_stat.st_gid ) ||
    		( cur->ll_info.i_stat.st_ctime != 
				tran->t_info.i_stat.st_ctime ) ||
    		( cur->ll_info.i_stat.st_size != 
				tran->t_info.i_stat.st_size ) || 
    		( cur->ll_info.i_chksum != tran->t_info.i_chksum ) ||
		( mode != tran_mode )) {
			t_printfs( &cur->ll_info, out, rpath );
    	}
	break;

    case S_IFDIR:				/* dir */
        if (( cur->ll_info.i_stat.st_uid != tran->t_info.i_stat.st_uid ) ||
	    	( cur->ll_info.i_stat.st_gid != tran->t_info.i_stat.st_gid ) ||
		( mode != tran_mode )) {
			t_printfs( &cur->ll_info, out, rpath );
        }
        break;

    case S_IFLNK:			    /* link */
        if ((( strcmp( cur->ll_info.i_link, tran->t_info.i_link )) != 0 )  ||
		( mode != tran_mode )) {
			t_printfs( &cur->ll_info, out, rpath );
	} 
	break;
    case S_IFCHR:
    case S_IFDOOR:
    case S_IFIFO:
    case S_IFSOCK:
	maj = major( cur->ll_info.i_stat.st_rdev );
	min = minor( cur->ll_info.i_stat.st_rdev );
	if (( cur->ll_info.i_stat.st_uid != tran->t_info.i_stat.st_uid ) ||
	    	( cur->ll_info.i_stat.st_gid != tran->t_info.i_stat.st_gid ) || 
	    	( maj != tran->t_info.i_maj ) || 
	    	( min != tran->t_info.i_min ) ||
		( mode != tran_mode )) {
			t_printfs( &cur->ll_info, out, rpath );
	}	
	break;
    default:
	fprintf( stderr, "ERROR: Incorrect file type\n" );
	break;
    }

    return 0;
}

    int
transcript( struct llist *new, char *name, char *rpath, FILE *out )
{

    int			move;
    int			pos = 1;
    int 		count;
    char		buf[ MAXPATHLEN ];
    char		temp[ MAXPATHLEN ];
    int			ret;
    int			comp;
    struct transcript	*t_cur = NULL;
    struct transcript	*begin_tran = NULL;

printf( "in tran\n" );

    if ( lstat( name, &new->ll_info.i_stat ) != 0 ) {
	perror( name );
	exit( 1 );
    }

    /* check to see if a link, then read it in */
    if ( S_ISLNK( new->ll_info.i_stat.st_mode )) {
	count = readlink( new->ll_info.i_name, buf, MAXPATHLEN );
	buf[ count ] = '\0';
	strcpy( new->ll_info.i_link, buf );
    }

    /* only go into the file if it is a directory */
    if ( S_ISDIR( new->ll_info.i_stat.st_mode )) {
	move = 1;
    } else { 
	move = 0;
    }

    for ( ; ; ) {
    	/* find the correct transcript to start with */
    	if ( tran_head != NULL ) {
	    /* loop through the list of transcripts and compare each
	       to find which transcript to start with */
	    for ( begin_tran = tran_head, t_cur = tran_head->t_next;
		    t_cur != NULL; t_cur = t_cur->t_next ) {
		/* compare heads of trans */
		if ( begin_tran->t_flag == T_EOF ) {
		    /* if the begin_tran head is eof, switch to the next 
		       transcript */
		    begin_tran = t_cur;
		    continue;
		}
		if ( t_cur->t_flag != T_EOF ) {
		    /* call strcmp on the two transcripts */
		    comp = strcmp( begin_tran->t_info.i_name, 
			    t_cur->t_info.i_name );
		    if ( comp > 0 ) {
			/* if t_cur is alphabetically before begin_tran,
			   set begin tran to t_cur */
			begin_tran = t_cur;
		    }
		}
	    }
     	}

     	/* move ahead other transcripts that match */
	if ( begin_tran != NULL ) {
     	   for( t_cur = begin_tran->t_next; t_cur != NULL; 
			t_cur = t_cur->t_next ) {
		if ( strcmp( begin_tran->t_info.i_name, t_cur->t_info.i_name ) 
					== 0 ) {
			t_parse( t_cur );
		}
     	   }
	}

	/* t_compare returns similar values as strcmp depending on
	   what the return value of the internal strcmp is */
	ret = t_compare( new, begin_tran, rpath, out );
	switch ( ret ) {
	case -1 :		/* either there is no transcript, or the
				   fs value is alphabetically before the
				   transcript value. move the fs forward. */
printf( "case -1: %d %s %s\n", move, begin_tran->t_info.i_name, new->ll_info.i_name );
	    return( move ? 1 : 0 );

	case 0 :		/* the two values match.  move ahead in both */
printf( "case 0: %d %s %s\n", move, begin_tran->t_info.i_name, new->ll_info.i_name );
	    /* t_compare() can't return 0 if begin_tran is NULL */
	    t_parse( begin_tran );
	    if ( begin_tran->t_type == T_NEGATIVE ) {
		return( 0 );
	    } else {
		return( move ? 1 : 0 );
	    }

	case 1 :		/* the fs value is alphabetically after the
				   transcript value.  move the transcript
				   forward */
printf( "case 1: %d %s %s\n", move, begin_tran->t_info.i_name, new->ll_info.i_name );
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

    return( new );
}

    void
transcript_init( )
{
    char	**av;
    char	line[ MAXPATHLEN ];

    tran_head = NULL;

    /* open the command file.  read in each line of the file and determine 
       which type of transcript it is.  */
    if (( com = fopen( "command", "r" )) != NULL ) {
	while ( fgets( line, sizeof( line ), com ) != NULL ) {
		argcargv( line, &av );
		tran_head = t_new( av[ 1 ], tran_head );
		switch( *av[0] ) {
		case 'p':
			tran_head->t_type = T_POSITIVE;
			break;
		case 'n':
			tran_head->t_type = T_NEGATIVE;
			break;
		case 's':
			tran_head->t_type = T_SPECIAL;
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
		strcpy( tran_head->t_name, av[ 1 ] );
		tran_head->t_flag = 1;
		/*parse first line of this transcript */
		t_parse( tran_head );
	}
     }

     return;
}


    void
transcript_free( )
{
    struct transcript    *next;
    FILE		 *com;

    if ( tran_head == NULL ) { 
	return;
    }

    for ( ; tran_head != NULL; tran_head = next ) {
	next = tran_head->t_next;
	fclose( tran_head->t_in );
	free( tran_head );
    }
    free( com );
}
